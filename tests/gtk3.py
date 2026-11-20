"""Run from the build directory with xvfb-run and a D-Bus session."""

import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
from xml.etree import ElementTree

import pyatspi


def descendants(node):
    yield node
    for child in node:
        yield from descendants(child)


def wait_for(predicate, description):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(0.1)
    raise AssertionError(f"Timed out waiting for {description}")


def widget(window, name=None, role=None):
    def find():
        for node in descendants(pyatspi.Registry.getDesktop(0)):
            if node.name != window or node.getRoleName() not in ("frame", "dialog", "alert"):
                continue
            for item in descendants(node):
                if (name is None or item.name == name) and (role is None or item.getRoleName() == role):
                    if item.getState().contains(pyatspi.STATE_SHOWING):
                        return item
        return None

    return wait_for(find, f"{role or 'widget'} {name!r} in {window!r}")


def click(window, name, role="push button"):
    assert widget(window, name, role).queryAction().doAction(0), name


def check_theme_staging(build, work):
    source = work / "theme-source"
    (source / "image/setup.data").mkdir(parents=True)
    for name in ("setup.gtk3", "image/setup.data/setup.gtk3.ui"):
        (source / name).symlink_to(build / name)

    css_path = "setup.data/setup.gtk3.css"
    default_theme = b".loki-setup { color: #1b3831; }\n"
    custom_theme = b".loki-setup { color: #12675b; }\n"
    (source / "image" / css_path).write_bytes(default_theme)
    media = work / "theme-media"
    stage = ["sh", build / "extras/stage-media.sh", media,
             "Linux", "test-arch", "test-libc", "install-image", "setup.gtk3"]
    env = dict(os.environ, STRIP_MEDIA="no")

    subprocess.run(stage, cwd=source, env=env, check=True)
    staged_theme = media / css_path
    assert staged_theme.read_bytes() == default_theme, "Staging should copy the source theme"

    staged_theme.write_bytes(custom_theme)
    subprocess.run(stage, cwd=source, env=env, check=True)
    assert staged_theme.read_bytes() == custom_theme, "Staging should preserve the media theme"


def run(build, setupdb, work):
    media = work / "media"
    installed = work / "installed files"
    registry = work / "registry"
    man_dir = work / "manuals"
    man_dir.mkdir()
    env = dict(os.environ, LC_ALL="C.UTF-8", NO_AT_BRIDGE="0", G_DEBUG="fatal-warnings",
               SETUPDB_ROOT=str(registry), SETUP_EXEC_TMPDIR=str(work), MANPATH=str(man_dir))
    for key in ("SETUP_GLADE_FILE", "SETUP_GTK3_UI_FILE", "SETUP_ARCH", "SETUPDB_XML_BASE"):
        env.pop(key, None)

    subprocess.run(["make", "install-image", f"IMAGE={media}", "CONVERT_IMAGE="],
                   cwd=build, check=True)
    check_theme_staging(build, work)
    shutil.copy2(build / "image/setup.sh", media)
    (media / "setup.data/config.sh").write_text("GET_ROOT=0\n")
    manifest = """<install product="gtk3-smoke" desc="GTK3 Smoke" version="1"
         manpages="yes">
  <eula>License.txt</eula>
  <readme>Readme.txt</readme>
  <require condition="+(gtk3,!gtk2)">This fixture requires GTK3.</require>
  <option install="true" required="true">Core
    <files>payload.tar.gz</files>
    <manpage name="gtk3-smoke" section="1" />
    <binary inline="true" binpath="gtk3-smoke">#!/bin/sh
exit 0
    </binary>
  </option>
  <option install="false" if="gtk3">Extras<eula>ExtraLicense.txt</eula><files>extra.txt</files></option>
  <option install="false">Skip me<files>skip.txt</files></option>
</install>
"""
    (media / "setup.data/setup.xml").write_text(manifest)
    payload = b"Original file from the installer archive\n"
    (media / "payload.txt").write_bytes(payload)
    with tarfile.open(media / "payload.tar.gz", "w:gz", format=tarfile.USTAR_FORMAT) as archive:
        archive.add(media / "payload.txt", arcname="payload.txt")
    (media / "payload.txt").unlink()
    (media / "extra.txt").write_text("Selected extra\n")
    (media / "skip.txt").write_text("Unselected extra\n")
    (media / "License.txt").write_text("Fixture license agreement.\n")
    (media / "ExtraLicense.txt").write_text("Optional extras license.\n")
    (media / "Readme.txt").write_text("Fixture installation notes.\n")
    man_page = Path("man/man1/gtk3-smoke.1")
    (media / man_page.parent).mkdir(parents=True)
    (media / man_page).write_text(".TH GTK3-SMOKE 1\n")
    wrong = work / "other media"
    shutil.copytree(media / "setup.data", wrong / "setup.data", ignore=shutil.ignore_patterns("bin"))
    (wrong / "setup.data/setup.xml").write_text(manifest.replace('version="1"', 'version="2"'))

    setup_binary, = (media / "setup.data/bin").glob("*/*/*/setup.gtk3")
    platform = setup_binary.parent.parent
    check_tools = media / "bin" / platform.parent.name / platform.name
    check_tools.mkdir(parents=True)
    vendor_tools = check_tools / "bin"
    vendor_tools.mkdir()
    uninstall_tools = media / "loki_uninstall/bin" / platform.name / setup_binary.parent.name
    uninstall_tools.mkdir(parents=True)
    updates = work / "updates"
    updates.mkdir()

    def stage_utilities(*options):
        subprocess.run(["make", *options, "install-check", "install-loki_uninstall",
                        "copy-loki_uninstall", f"IMAGE={media}", f"UPDATES={updates}"],
                       cwd=build, check=True)

    stage_utilities()

    no_display = dict(env, GDK_BACKEND="x11", DISPLAY="")
    assert subprocess.run([setup_binary], cwd=media, env=no_display).returncode == 4
    missing_layout = dict(env, SETUP_GTK3_UI_FILE="missing.ui")
    assert subprocess.run([setup_binary], cwd=media, env=missing_layout).returncode == 4
    layout = ElementTree.parse(media / "setup.data/setup.gtk3.ui")
    agree = layout.find(".//object[@id='license_agree']")
    agree.set("id", "unused_button")
    incomplete = work / "incomplete.ui"
    layout.write(incomplete, encoding="utf-8")
    assert subprocess.run([setup_binary], cwd=media,
                          env=dict(env, SETUP_GTK3_UI_FILE=str(incomplete))).returncode == 4
    agree.set("id", "license_agree")
    book = layout.find(".//object[@id='setup_notebook']")
    options = book.find("child/object[@id='options_page']/..")
    book.remove(options)
    book.append(options)
    reordered = work / "reordered.ui"
    layout.write(reordered, encoding="utf-8")
    env["SETUP_GTK3_UI_FILE"] = str(reordered)

    processes = []
    logs = []

    def launch(*args, cwd):
        log = tempfile.TemporaryFile()
        logs.append(log)
        proc = subprocess.Popen(args, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT,
                                start_new_session=True)
        processes.append(proc)
        return proc

    try:
        css = media / "setup.data/setup.gtk3.css"
        theme = "window.loki-setup > box { min-width: 790px; }\n"

        def license_width():
            license_window = widget("License Agreement", "License Agreement", "frame")
            return license_window.queryComponent().getExtents(pyatspi.DESKTOP_COORDS).width

        baseline = launch("./setup.sh", "-i", str(installed), cwd=media)
        system_width = license_width()
        click("License Agreement", "Cancel")
        assert baseline.wait(timeout=10) == 0

        css.write_text(theme + "window { unknown-property: 1; }\n")
        invalid_theme = launch("./setup.sh", "-i", str(installed), cwd=media)
        assert license_width() == system_width
        click("License Agreement", "Cancel")
        assert invalid_theme.wait(timeout=10) == 0
        logs[-1].seek(0)
        assert b"Ignoring stylesheet setup.data/setup.gtk3.css" in logs[-1].read()

        css.write_text(theme)
        setup = launch("./setup.sh", "-i", str(installed), cwd=media)
        assert license_width() >= 790
        click("License Agreement", "Agree")
        click("GTK3 Smoke Setup", "View Readme")
        click("Readme", "Close")
        menu = widget("GTK3 Smoke Setup", "Startup menu entries", "check box")
        assert menu.getState().contains(pyatspi.STATE_CHECKED)
        window = widget("GTK3 Smoke Setup", "GTK3 Smoke Setup", "frame")
        man_entry = next((item for item in descendants(window) if item.getRoleName() == "text"
                          and item.queryText().getText(0, -1) == str(man_dir)), None)
        assert man_entry is not None, "The default man-page path should be visible"
        assert man_entry.queryComponent().grabFocus()
        assert menu.queryComponent().grabFocus()
        click("GTK3 Smoke Setup", "View Readme")
        click("Readme", "Close")
        def select_extras():
            # Dispatch a keyboard event so AT-SPI remains available while
            # the option callback waits for a license response.
            extra = widget("GTK3 Smoke Setup", "Extras", "check box")
            window_id = subprocess.check_output(
                ["xdotool", "search", "--onlyvisible", "--name", "^GTK3 Smoke Setup$"],
                text=True).strip()
            subprocess.run(["xdotool", "windowfocus", "--sync", window_id], check=True)
            assert extra.queryComponent().grabFocus()
            subprocess.run(["xdotool", "key", "space"], check=True)

        select_extras()
        click("License Agreement", "Cancel")
        extra = widget("GTK3 Smoke Setup", "Extras", "check box")
        wait_for(lambda: not extra.getState().contains(pyatspi.STATE_CHECKED), "declined option")
        select_extras()
        click("License Agreement", "Agree")
        click("GTK3 Smoke Setup", "Begin Install")
        click("GTK3 Smoke Setup", "Quit")
        assert setup.wait(timeout=10) == 0
        assert (installed / "payload.txt").read_bytes() == payload
        assert (installed / "extra.txt").read_text() == "Selected extra\n"
        man_link = man_dir / "man1" / man_page.name
        assert man_link.is_symlink() and man_link.resolve(strict=True) == installed / man_page
        assert not (installed / "skip.txt").exists()
        files = subprocess.check_output([setupdb, "gtk3-smoke", "listfiles"], env=env, text=True)
        assert str(installed / "payload.txt") in files
        assert str(installed / "extra.txt") in files

        (installed / "payload.txt").write_text("Modified file\n")
        (installed / "extra.txt").unlink()
        check = launch("./check.gtk3", "gtk3-smoke", cwd=check_tools)
        widget("Check Installed Files", "Changes detected: 1 files removed, 1 files modified.", "label")

        def rescue_from(directory):
            click("Check Installed Files", "Rescue")
            click("Select Installation Media", "Other directory:", "radio button")
            widget("Select Installation Media", role="text").queryEditableText().setTextContents(str(directory))
            click("Select Installation Media", "OK")

        rescue_from(wrong)
        widget("Check Installed Files", "Installation media doesn't match the product.", "label")
        assert (installed / "payload.txt").read_text() == "Modified file\n"
        assert not (installed / "extra.txt").exists()
        rescue_from(media)
        widget("Check Installed Files", "Files successfully restored!", "label")
        assert (installed / "payload.txt").read_bytes() == payload
        assert (installed / "extra.txt").read_text() == "Selected extra\n"
        click("Check Installed Files", "Close")
        assert check.wait(timeout=10) == 0

        uninstall = launch("./loki_uninstall.gtk3", cwd=uninstall_tools)
        click("Loki Uninstall", "Complete uninstall", "check box")
        click("Loki Uninstall", "Uninstall")
        widget("Loki Uninstall", "Uninstall complete", "label")
        click("Loki Uninstall", "Quit")
        assert uninstall.wait(timeout=10) == 0
        assert not (installed / "payload.txt").exists()
        assert not (installed / "extra.txt").exists()
        assert not os.path.lexists(registry / "installed/gtk3-smoke.xml")

        update_bin, = updates.glob("bin-*")
        vendor_uninstaller, = (vendor_tools / "gtk3").glob("*-uninstall")
        gtk3_tools = (check_tools / "check.gtk3", uninstall_tools / "loki_uninstall.gtk3",
                      update_bin / "loki_uninstall.gtk3", vendor_uninstaller)
        assert all(path.is_file() for path in gtk3_tools)
        keep = vendor_uninstaller.with_name("package-notes")
        keep.write_text("Media-owned content")
        if (build / "check.gtk2").is_file():
            stage_utilities("GTK_VERSIONS=gtk2", "-W", "check.gtk2", "-W", "loki_uninstall.gtk2")
            assert (check_tools / "check.gtk2").is_file()
            assert (uninstall_tools / "loki_uninstall.gtk2").is_file()
            assert all(not path.exists() for path in gtk3_tools)
        stage_utilities("GTK_VERSIONS=")
        assert all(not path.exists() for path in gtk3_tools)
        assert not (check_tools / "check").exists()
        assert not (uninstall_tools / "loki_uninstall").exists()
        assert keep.read_text() == "Media-owned content"
    except Exception:
        for node in descendants(pyatspi.Registry.getDesktop(0)):
            if node.name and node.getState().contains(pyatspi.STATE_SHOWING):
                print(node.getRoleName(), repr(node.name), file=sys.stderr)
        for proc, log in zip(processes, logs):
            log.seek(0)
            print(f"{proc.args}: {log.read().decode(errors='replace')}", file=sys.stderr)
        raise
    finally:
        for proc in processes:
            if proc.poll() is None:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait(timeout=5)
        for log in logs:
            log.close()


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="loki-gtk3-") as directory:
        run(Path.cwd(), Path(sys.argv[1]).resolve(), Path(directory).resolve())
    print("GTK3 theme staging, install, verification, rescue and uninstall passed")
