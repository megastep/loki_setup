#include "carbon/carbonres.h"
#include "carbon/carbondebug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "setupdb.h"
#include "uninstall.h"
#include "uninstall_carbonui.h"

static int uninstall_cancelled = 0;
static CarbonRes *Res;
static OptionsBox *Box;

// List of open products that need closing
struct product_list {
    product_t *product;
    struct product_list *next;
} *product_list = NULL;

// List of components and associated widgets
typedef struct {
    product_t *product;
    product_info_t *info;
    product_component_t *component;
    size_t size;
    struct component_button {
        OptionsButton *widget;
        struct component_button *next;
    } *buttons;
} component_list;

static void add_product(product_t *product)
{
    carbon_debug("add_products()\n");
    struct product_list *entry;

    entry = (struct product_list *)malloc(sizeof *entry);
    if ( entry ) {
        entry->product = product;
        entry->next = product_list;
        product_list = entry;
    }
}

static void remove_product(product_t *product)
{
    carbon_debug("remove_products()\n");

    struct product_list *prev, *entry;

    prev = NULL;
    for ( entry = product_list; entry; entry = entry->next ) {
        if ( entry->product == product ) {
            if ( prev ) {
                prev->next = entry->next;
            } else {
                product_list = entry->next;
            }
            free(entry);
            break;
        }
    }
}

static size_t calculate_recovered_space(void)
{
    OptionsButton *button;
    int i;
    component_list *component;
    int ready;
    size_t size;

    ready = false;
    size = 0;

    for(i = 0; i < Box->ButtonCount; i++)
    {
        button = Box->Buttons[i];
        if(button->Type == ButtonType_Checkbox && carbon_OptionsGetValue(button))
        {
            component = (component_list *)button->Data;
            size += component->size / 1024;
            ready = true;
        }
    }
    
    char text[128];
    sprintf(text, "%ld MB", size/1024);
    carbon_SetLabelText(Res, UNINSTALL_SPACE_VALUE_LABEL_ID, text);

    if(ready)
        carbon_EnableControl(Res, UNINSTALL_UNINSTALL_BUTTON_ID);
    else
        carbon_DisableControl(Res, UNINSTALL_UNINSTALL_BUTTON_ID);

    return(size);
}

static void close_products(int status)
{
    carbon_debug("close_products()\n");

    struct product_list *freeable;

    while ( product_list ) {
        freeable = product_list;
        product_list = product_list->next;
        loki_closeproduct(freeable->product);
        free(freeable);
    }
}

static component_list *create_component_list(product_t *product,
                                             product_info_t *info,
                                             product_component_t *component)
{
    carbon_debug("create_component_list()\n");

    component_list *list;

    list = (component_list *)malloc(sizeof *list);
    if ( list ) {
        list->product = product;
        list->info = info;
        list->component = component;
        list->size = loki_getsize_component(component);
        list->buttons = NULL;
    }
    return(list);
}

static void add_component_list(component_list *list, OptionsButton *widget)
{
    carbon_debug("add_component_list()\n");

    struct component_button *entry;

    if ( list ) {
        entry = (struct component_button *)malloc(sizeof *entry);
        if ( entry ) {
            entry->widget = widget;
            entry->next = list->buttons;
            list->buttons = entry;
        }
    }
}

void OnCommandExit()
{
    carbon_debug("OnCommandExit()\n");
    uninstall_cancelled = 1;
    QuitApplicationEventLoop();
}

void OnCommandCancel()
{
    carbon_debug("OnCommandCancel()\n");
    uninstall_cancelled = 1;
}

void main_signal_abort(int status)
{
    carbon_debug("main_signal_abort()\n");
    OnCommandExit();
}

OptionsButton *GoToNextProduct(OptionsButton *ProductButton)
{
    carbon_debug("GoToNextProduct()\n");

    OptionsButton *Button = ProductButton;
    int ExitTime;

    // If button is not a valid button, set it to first in box
    if(Button == NULL)
        Button = Box->Buttons[0];

    ExitTime = 0;
    while(!ExitTime)
    {
        // Label signifies the beginning of a new group
        if(Button->Type == ButtonType_Label)
            ExitTime = true;
        // Go to the next button.  If current one is a label then
        //  this one will be the first check button in the group
        Button = Button->NextButton;
        // If button is NULL, we're at the end of the list
        if(Button == NULL)
            ExitTime = true;
    }

    // Return the first button in the next product (or NULL for end)
    return Button;
}

OptionsButton *GoToNextComponent(OptionsButton *ComponentButton)
{
    carbon_debug("GoToNextComponent()\n");

    OptionsButton *Button = ComponentButton;

    if(Button == NULL)
    {
        carbon_debug("GoToNextComponent() - ComponentButton shouldn't be NULL\n");
        return NULL;
    }

    // Go to the next button.
    Button = Button->NextButton;
    // Separator signifies the end of components for that group
    if(Button->Type == ButtonType_Separator)
        Button = NULL;

    // Return the first button in the next product (or NULL for end)
    return Button;
}
  
//void perform_uninstall_slot(GtkWidget* w, gpointer data)
void OnCommandUninstall()
{
    carbon_debug("OnCommandUninstall()\n");

    OptionsButton *button, *productbutton = NULL;
    component_list *component;
    size_t size, total;
    char text[1024];
	const char *message;

    // First switch to the next notebook page
    carbon_SetProperWindowSize(Box, false);
    carbon_ShowInstallScreen(Res, UNINSTALL_STATUS_PAGE);
    // Disable finish button
    carbon_DisableControl(Res, UNINSTALL_STATUS_FINISHED_BUTTON_ID);

    // Now uninstall all the selected components
    size = 0;
    total = calculate_recovered_space();
    
    // Go to first checkbox button in next product
    while((productbutton = GoToNextProduct(productbutton)))
    {
        //***GO THROUGH ADDON COMPONENTS FIRST***
        // Set button to first checkbox in current product
        button = productbutton;
        do
        {
            if(button->Type == ButtonType_Checkbox && carbon_OptionsGetValue(button))
            {
                component = (component_list *)button->Data;

                if(loki_isdefault_component(component->component))
                    continue;

                // Put up the status
                snprintf(text, sizeof(text), "%s: %s",
                        component->info->description,
                        loki_getname_component(component->component));
                carbon_SetLabelText(Res, UNINSTALL_STATUS_OPTION_LABEL_ID, text);
                //set_status_text(text);

                // See if the user wants to cancel the uninstall
                carbon_HandlePendingEvents(Res);
    			
			    // Display an optional message to the user
			    message = loki_getmessage_component(component->component);
                if(message && !carbon_Prompt(Res, PromptType_OKAbort, message))
                {
                    //clist = clist->next;
				    uninstall_cancelled = 1;
				    break;
			    }

                // Remove the component
                if(!uninstall_component(component->component, component->info))
                {
				    uninstall_cancelled = 2;
				    snprintf(text, sizeof(text),
                        "Uninstallation of component %s has failed!  The whole uninstallation may be incomplete.",
				        loki_getname_component(component->component));
				    carbon_Prompt(Res, PromptType_OK, text);
				    break;
			    }

                // Update the progress bar
                if(total)
                {
                    size += component->size/1024;
                    carbon_SetProgress(Res, UNINSTALL_STATUS_PROGRESS_ID, (float)size/total);
                }
        
                DisableControl(button->Control);
                //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);

                // See if the user wants to cancel the uninstall
                carbon_HandlePendingEvents(Res);
            }
        } while((button = GoToNextComponent(button)));

        //***GO THROUGH PRIMARY COMPONENTS***
        // Set button to first checkbox in current product
        button = productbutton;
        do
        {
            if(button->Type == ButtonType_Checkbox && carbon_OptionsGetValue(button))
            {
                component = (component_list *)button->Data;

                if(!loki_isdefault_component(component->component))
                    continue;

                // Put up the status
                strncpy(text, component->info->description, sizeof(text));
                carbon_SetLabelText(Res, UNINSTALL_STATUS_OPTION_LABEL_ID, text);
                //set_status_text(text);

                // See if the user wants to cancel the uninstall
                carbon_HandlePendingEvents(Res);
    			
			    // Display an optional message to the user
			    message = loki_getmessage_component(component->component);
                if(message && !carbon_Prompt(Res, PromptType_OKAbort, message))
                {
                    //clist = clist->next;
				    uninstall_cancelled = 1;
				    break;
			    }

                // Remove the component
                if(!perform_uninstall(component->product, component->info))
                {
				    uninstall_cancelled = 2;
				    snprintf(text, sizeof(text),
                        "Uninstallation of product %s has failed!  Aborting the rest of the uninstallation.",
 						component->info->description);

				    carbon_Prompt(Res, PromptType_OK, text);
				    break;
			    }

                remove_product(component->product);

                // Update the progress bar
                if(total)
                {
                    size += component->size/1024;
                    carbon_SetProgress(Res, UNINSTALL_STATUS_PROGRESS_ID, (float)size/total);
                }
        
                //DisableControl(button->Control);
                //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);

                // See if the user wants to cancel the uninstall
                carbon_HandlePendingEvents(Res);

                break;
            }
        } while((button = GoToNextComponent(button)));
    } // end go to next product

    switch(uninstall_cancelled)
    {
	    case 1:
            carbon_SetLabelText(Res, UNINSTALL_STATUS_OPTION_LABEL_ID, "Uninstall cancelled");
            //set_status_text("Uninstall cancelled");
		    break;
	    case 2:
            carbon_SetLabelText(Res, UNINSTALL_STATUS_OPTION_LABEL_ID, "Uninstall aborted");
            //set_status_text("Uninstall aborted");
		    break;
	    default:
            carbon_SetLabelText(Res, UNINSTALL_STATUS_OPTION_LABEL_ID, "Uninstall complete");
            //set_status_text("Uninstall complete");
		    break;
    }

    carbon_DisableControl(Res, UNINSTALL_STATUS_CANCEL_BUTTON_ID);
    carbon_EnableControl(Res, UNINSTALL_STATUS_FINISHED_BUTTON_ID);
}

int OnCommandEvent(UInt32 CommandID)
{
    int ReturnValue = false;

    carbon_debug("OnCommandEvent()\n");

    switch(CommandID)
    {
        case COMMAND_EXIT:
            OnCommandExit();
            ReturnValue = true;
            break;
        case COMMAND_UNINSTALL:
            OnCommandUninstall();
            break;
        case COMMAND_CANCEL:
        case COMMAND_FINISHED:
            OnCommandCancel();
            break;
        default:
            carbon_debug("OnCommandEvent() - Invalid command received.\n");
            break;
    }

    return ReturnValue;
}

int OnOptionClickEvent(OptionsButton *w)
{
    static int in_component_toggled_slot = 0;
    component_list *list = (component_list *)w->Data;
    int state;
    struct component_button *button;

    // Prevent recursion
    if(in_component_toggled_slot)
        return true;

    in_component_toggled_slot = 1;

    // Set the state for any linked components
    //state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
    state = carbon_OptionsGetValue(w);
    for(button = list->buttons; button; button = button->next)
    {
        //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button->widget), state);
        carbon_OptionsSetValue(button->widget, state);
        if(state)
            DisableControl(button->widget->Control);
            //gtk_widget_set_sensitive(button->widget, FALSE);
        else
            DisableControl(button->widget->Control);
            //gtk_widget_set_sensitive(button->widget, TRUE);
    }

    // Calculate recovered space, and we're done
    calculate_recovered_space();
    in_component_toggled_slot = 0;

    return true;
}

// Run a GUI to select and uninstall products
int uninstall_ui(int argc, char *argv[])
{
    carbon_debug("uninstall_ui()\n");

    OptionsButton *button;
    const char *product_name;
    product_t *product;
    product_info_t *product_info;
    product_component_t *component;
    component_list *component_list, *addon_list;
    char text[1024];

    // Load resource data
    Res = carbon_LoadCarbonRes(OnCommandEvent);
    Box = carbon_OptionsNewBox(Res, false, OnOptionClickEvent);

    // Add emergency signal handlers
    /*signal(SIGHUP, main_signal_abort);
    signal(SIGINT, main_signal_abort);
    signal(SIGQUIT, main_signal_abort);
    signal(SIGTERM, main_signal_abort);*/

    for(product_name=loki_getfirstproduct();
        product_name;
        product_name=loki_getnextproduct())
    {
        // See if we can open the product
        product = loki_openproduct(product_name);
        if(!product)
            continue;

        // See if we have permissions to remove the product
        product_info = loki_getinfo_product(product);
        if(!check_permissions(product_info, 0))
        {
            loki_closeproduct(product);
            continue;
        }

        // Add the product and components to our list
        strncpy(text, product_info->description, sizeof(text));
        carbon_OptionsNewLabel(Box, text);
        component = loki_getdefault_component(product);
        component_list = NULL;
        if(component)
        {
            component_list = create_component_list(product, product_info,
                                                   component);
            strncpy(text, "Complete uninstall", sizeof(text));
            button = carbon_OptionsNewCheckButton(Box, text);
            button->Data = (void *)component_list;

        }
        for(component = loki_getfirst_component(product); 
            component;
            component = loki_getnext_component(component))
        {
            if(loki_isdefault_component(component))
                continue;

            addon_list = create_component_list(product, product_info,
                component);
            strncpy(text, loki_getname_component(component), sizeof(text));

            button = carbon_OptionsNewCheckButton(Box, text);
            button->Data = (void *)addon_list;
            add_component_list(component_list, button);
        }

        // Add separator between products
        carbon_OptionsNewSeparator(Box);

        // Add this product to our list of open products
        add_product(product);
    }

    // Check to make sure there's something to uninstall
    if(!product_list)
        carbon_Prompt(Res, PromptType_OK, "No products were installed by this user.  You may need to run this tool as an administrator.");
    else
    {
        // Render and display options in window
        carbon_OptionsShowBox(Box);
        // Resize the window if there are too many options to fit
        carbon_SetUninstallWindowSize(Box);
        // Show the uninstall screen
        carbon_ShowInstallScreen(Res, UNINSTALL_PAGE);
        // Wait for user input
        carbon_IterateForState(Res, &uninstall_cancelled); 
    }

    // Close all the products and return
    close_products(0);
    return 0;
}