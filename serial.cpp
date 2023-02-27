//g++ $( pkg-config --cflags gtk4 ) -o serial serial.cpp serialib/serialib.cpp $( pkg-config --libs gtk4 )
//sudo dnf install gtk4-devel    sudo apt install libgtk-4-dev

#include <gtk/gtk.h>
#include "serialib/serialib.h" //http://lucidar.me/en/serialib/cross-plateform-rs232-serial-library/

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

struct Widgets {
    GtkWidget *labelSerialPort;
    GtkWidget *labelBaudRate;
    GtkWidget *entrySerialPort;
    GtkWidget *entryBaudRate;
    GtkWidget *buttonConnect;
    GtkWidget *checkDTR;
    GtkWidget *checkRTS;
    GtkWidget *checkTimeStamp;
    GtkWidget *comboNewLine;
    GtkWidget *checkSaveLog;
    GtkWidget *entryCommand;
    GtkWidget *buttonSaveLogAs;
    GtkWidget *buttonClear;
    GtkWidget *receivedLogs;
    GtkTextBuffer *buffer;
} widgets;

serialib serial;                                                          // Object of the serialib class
int Ret;                                                                // Used for return values
char Buffer[128];
static gboolean readSerial(gpointer user_data);
void setKeyFile();
void getKeyFile();

std::string receivedData, logFileUri;

std::string getFormatedTime(char const *format);

static void file_picked_cb (GtkDialog *dialog, int response)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        g_autoptr(GFile) file = gtk_file_chooser_get_file (chooser);
        char *filename = gtk_file_chooser_get_current_name (chooser);
        char *fileUri = g_file_get_uri (file);
        logFileUri = fileUri;
        logFileUri = logFileUri.substr(7);
        gtk_check_button_set_label (GTK_CHECK_BUTTON(widgets.checkSaveLog), filename);
        std::cout << logFileUri<<"\n";
        
        //save_to_file (file);
        g_object_unref(file);
        g_free(fileUri);
        g_free(filename);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(widgets.buttonSaveLogAs), TRUE);
    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void show_save_dialog (GtkWidget *widget, gpointer data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(widgets.buttonSaveLogAs), FALSE);
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File",
                                                    NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Save", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    GtkFileFilter *filter = gtk_file_filter_new(); // filter 1
    gtk_file_filter_set_name(filter, "Plain text (.txt)");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    filter = gtk_file_filter_new(); // filter 2
    gtk_file_filter_set_name(filter, "All files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    const gchar *fileName = gtk_check_button_get_label (GTK_CHECK_BUTTON(widgets.checkSaveLog));
    if(fileName == 0 || strcmp(fileName, "Save log") == 0)
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), ".txt");
    else
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), fileName);
    g_signal_connect(dialog, "response", G_CALLBACK(file_picked_cb), data);
    gtk_window_present (GTK_WINDOW (dialog));
}

static void connectSerial(GtkWidget *widget, Widgets *widg){
    if(!serial.isDeviceOpen()){
        GtkEntry *entrySerialPort = GTK_ENTRY (widg->entrySerialPort);
        const char* entry_textSerialPort = gtk_entry_buffer_get_text (gtk_entry_get_buffer(GTK_ENTRY(entrySerialPort)));
        
        GtkEntry *entryBaudRate = GTK_ENTRY (widg->entryBaudRate);
        const char* entry_textBaudRate = gtk_entry_buffer_get_text (gtk_entry_get_buffer(GTK_ENTRY(widg->entryBaudRate)));
        
        Ret=serial.openDevice(entry_textSerialPort, atoi(entry_textBaudRate));                                        // Open serial link
        if (Ret!=1) {                                                           // If an error occured...
            std::cout << "Error while opening port. Permission problem?\n";        // ... display a message ...
            return;                                                         // ... quit the application
        }
        std::cout << "Serial port opened successfully!\n";
        gtk_button_set_label(GTK_BUTTON(widgets.buttonConnect), "Disconnect");
        g_timeout_add (100 /* milliseconds */, readSerial, NULL);
        setKeyFile();
    }
    else
    {
        serial.closeDevice();
        gtk_button_set_label(GTK_BUTTON(widgets.buttonConnect), "Connect");
        //g_source_remove_by_funcs_user_data (readSerial, NULL);
    }
}

static gboolean readSerial(gpointer user_data){
    if(serial.isDeviceOpen() == 0) {
        gtk_button_set_label(GTK_BUTTON(widgets.buttonConnect), "Connect");
        return FALSE;
    }
    Ret=serial.readString(Buffer,'\n',128,50);                                // Read a maximum of 128 characters with a timeout of 5 seconds
    // The final character of the string must be a line feed ('\n')
    if (Ret>0){
        if(gtk_check_button_get_active (GTK_CHECK_BUTTON(widgets.checkSaveLog))){
            std::ofstream myfile (logFileUri, std::ios_base::app);
            if (myfile.is_open())
            {
                bool foundNewLine = 0;
                for(int i = 0; i< 128, Buffer[i]!=0; i++){
                    if((Buffer[i]== '\n' && Buffer[i+1]==0) || (Buffer[i]== '\r' && Buffer[i+1]==0)) {
                        foundNewLine = 1;
                        break;
                    }
                }
                myfile << ( (gtk_check_button_get_active (GTK_CHECK_BUTTON(widgets.checkTimeStamp))) ? getFormatedTime("%d.%m.%Y %H:%M:%S > ") : "" )  << Buffer << ( (gtk_check_button_get_active (GTK_CHECK_BUTTON(widgets.checkTimeStamp)) && foundNewLine == 0) ? "\n"  : "" );
                myfile.close();
            } else std::cout << "Error writing file " << logFileUri; 
        }
        receivedData = ( (gtk_check_button_get_active (GTK_CHECK_BUTTON(widgets.checkTimeStamp))) ? getFormatedTime("%H:%M:%S > ") : "" ) + std::string(Buffer) + receivedData;
        /*int i, found = 0;
         *     f * or(i* = 0; i < receivedData.length(); i+*+)
         *     {
         *     if(receivedData[i] == '\n') {found++;
         *     if(found == 27){break;}
    }
    }
    if(i != receivedData.length() - 1){
        receivedData = receivedData.substr (0,i);
    }*/
        gtk_text_buffer_set_text (widgets.buffer, receivedData.c_str() , -1);
        
        /*GtkTextIter begiter;
         *		gtk_text_buffer_get_start_iter (widgets.buffer, &begiter);
         *		gtk_text_buffer_insert (widgets.buffer, &begiter,Buffer,-1);*/
        
        //std::time_t currentTime = std::time(nullptr);
        std::cout << Buffer;  // If a string has been read from, print the string
        //logfile << currentTime << " >> " << Buffer << std::flush;
    } else {
        //std::cout << "TimeOut reached. No data received !\n";                   // If not, print a message.
    }
    return G_SOURCE_CONTINUE; /* or G_SOURCE_REMOVE when you want to stop */
}

static void activate (GtkApplication* app, gpointer user_data)
{
    widgets.labelSerialPort = gtk_label_new ("Serial port");
    widgets.labelBaudRate = gtk_label_new ("Baud Rate");
    widgets.entrySerialPort = gtk_entry_new ();
    widgets.entryBaudRate = gtk_entry_new ();
    widgets.buttonConnect = gtk_button_new_with_label ("Connect");
    
    widgets.checkDTR = gtk_check_button_new_with_label ("DTR");
    g_signal_connect (widgets.checkDTR, "toggled", G_CALLBACK (+[](GtkWidget *widget, gpointer data) {
        serial.DTR(gtk_check_button_get_active (GTK_CHECK_BUTTON(widgets.checkDTR)));
    }), NULL);
    
    widgets.checkRTS = gtk_check_button_new_with_label ("RTS");
    g_signal_connect (widgets.checkRTS, "toggled", G_CALLBACK (+[](GtkWidget *widget, gpointer data) {
        serial.RTS(gtk_check_button_get_active (GTK_CHECK_BUTTON(widgets.checkRTS)));
    }), NULL);
    
    widgets.checkTimeStamp = gtk_check_button_new_with_label ("Time stamp");
    g_signal_connect (widgets.checkTimeStamp, "toggled", G_CALLBACK (+[](GtkWidget *widget, gpointer data) {
        setKeyFile();
    }), NULL);
    widgets.checkSaveLog = gtk_check_button_new_with_label ("Save log");
    widgets.entryCommand = gtk_entry_new ();
    widgets.receivedLogs = gtk_text_view_new ();
    widgets.buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widgets.receivedLogs));
    
    #if defined (_WIN32) || defined(_WIN64)
    #define SERIAL_PORT "\\\\.\\COM1"
    #endif
    #if defined (__linux__) || defined(__APPLE__)
    #define SERIAL_PORT "/dev/ttyUSB0"
    #endif
    gtk_entry_buffer_set_text (gtk_entry_get_buffer(GTK_ENTRY (widgets.entrySerialPort)), SERIAL_PORT, -1);
    gtk_entry_buffer_set_text (gtk_entry_get_buffer(GTK_ENTRY (widgets.entryBaudRate)), "9600", 4);
    gtk_entry_set_placeholder_text (GTK_ENTRY(widgets.entryCommand),"Send command");
    g_signal_connect(widgets.entryCommand, "activate", G_CALLBACK(+[](GtkEntry *entry, gpointer user_data) {
        if(!serial.isDeviceOpen()){return;}
        const char* command = gtk_entry_buffer_get_text (gtk_entry_get_buffer(entry));
        //g_print ("%s %i\n", command, gtk_drop_down_get_selected(GTK_DROP_DOWN(widgets.comboNewLine)));
        serial.writeString(command);
        switch(gtk_drop_down_get_selected(GTK_DROP_DOWN(widgets.comboNewLine))){
            case 1:
                serial.writeChar('\n');
                break;
            case 2:
                serial.writeChar('\r');
                break;
            case 3:
                serial.writeString("\r\n");
                break;
        }
        gtk_entry_buffer_set_text (gtk_entry_get_buffer(entry), "", 0);
        setKeyFile();
    }), NULL);
    gtk_widget_set_hexpand(widgets.buttonConnect, TRUE);
    
    g_signal_connect (widgets.buttonConnect, "clicked", G_CALLBACK (connectSerial), &widgets);
    
    GtkWidget *window;
    
    window = gtk_application_window_new (app);
    GtkWidget *grid = gtk_grid_new ();
    gtk_window_set_child (GTK_WINDOW (window), grid);
    
    gtk_window_set_title (GTK_WINDOW (window), "Serial monitor");
    gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
    //gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
    gtk_grid_attach (GTK_GRID (grid), widgets.labelSerialPort, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), widgets.labelBaudRate, 1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), widgets.entrySerialPort, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), widgets.entryBaudRate, 1, 1, 1, 1);
    
    GtkStringList *strings;
    strings = gtk_string_list_new(NULL);
    gtk_string_list_append (strings, "No line ending");
    gtk_string_list_append (strings, "\\n");
    gtk_string_list_append (strings, "\\r");
    gtk_string_list_append (strings, "\\r\\n");
    
    //https://github.com/GNOME/gtk/blob/main/demos/gtk-demo/font_features.c
    widgets.comboNewLine = gtk_drop_down_new(G_LIST_MODEL (strings), NULL);
    
    PangoLayout *pango = gtk_widget_create_pango_layout (widgets.comboNewLine, "No line ending");
    PangoRectangle logical_rect;
    pango_layout_get_pixel_extents(pango, NULL, &logical_rect);
    int minimum_width;
    gtk_widget_measure (widgets.comboNewLine, GTK_ORIENTATION_HORIZONTAL, -1, &minimum_width, NULL, NULL, NULL);
    g_object_unref(pango);
    gtk_widget_set_size_request (widgets.comboNewLine, logical_rect.width + minimum_width,-1);
    
    widgets.buttonSaveLogAs = gtk_button_new_with_label ("Save log as");
    g_signal_connect (widgets.buttonSaveLogAs, "clicked", G_CALLBACK (show_save_dialog), NULL);
    
    widgets.buttonClear = gtk_button_new_with_label ("Clear");
    g_signal_connect (widgets.buttonClear, "clicked", G_CALLBACK (+[](GtkWidget *widget, gpointer data) {
        gtk_text_buffer_set_text (widgets.buffer,"", 0);
        receivedData.clear();
    }
    ), NULL);
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(box), widgets.buttonConnect);
    gtk_box_append(GTK_BOX(box), widgets.checkDTR);
    gtk_box_append(GTK_BOX(box), widgets.checkRTS);
    gtk_box_append(GTK_BOX(box), widgets.checkTimeStamp);
    gtk_box_append(GTK_BOX(box), widgets.comboNewLine);
    gtk_box_append(GTK_BOX(box), widgets.checkSaveLog);
    gtk_box_append(GTK_BOX(box), widgets.buttonSaveLogAs);
    gtk_box_append(GTK_BOX(box), widgets.buttonClear);
    gtk_box_set_homogeneous(GTK_BOX(box), TRUE);
    gtk_grid_set_row_homogeneous(GTK_GRID (grid), TRUE);
    
    gtk_grid_attach (GTK_GRID (grid), box, 0, 2, 2, 1);
    gtk_grid_attach (GTK_GRID (grid), widgets.entryCommand, 0, 3, 2, 1);
    
    GtkWidget *scrolledwindow = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scrolledwindow), widgets.receivedLogs);
    gtk_grid_attach (GTK_GRID (grid), scrolledwindow, 0, 4, 2, 15);
    gtk_widget_set_can_focus (widgets.receivedLogs,FALSE);
    getKeyFile();
    gtk_widget_show (window);
}

int main (int argc, char **argv)
{
    GtkApplication *app;
    int status;
    
    app = gtk_application_new ("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);
    
    return status;
}

void getKeyFile(){
    g_autoptr(GKeyFile) key_file = g_key_file_new ();
    g_autoptr(GError) error = NULL;
    gchar *contents;
    gsize length;
    gboolean boolval;
    gint intval;
    gdouble doubleval;
    gchar *stringval, *stringval2;
    
    /* Load the file contents into a string */
    if (!g_file_get_contents("settings.ini", &contents, &length, &error)) {
        //g_error("Failed to load file: %s", error->message);
    }
    
    /* Parse the contents of the file */
    key_file = g_key_file_new();
    
    if (!g_key_file_load_from_data(key_file, contents, length, G_KEY_FILE_NONE, &error)) {
        return;
        //g_error("Failed to parse file: %s", error->message);
    }
    
    stringval = g_key_file_get_string(key_file, "Settings", "SerialPort", &error);
    if (error == NULL) {
        gtk_entry_buffer_set_text (gtk_entry_get_buffer(GTK_ENTRY (widgets.entrySerialPort)), stringval, -1);
    }
    
    stringval2 = g_key_file_get_string(key_file, "Settings", "BaudRate", &error);
    if (error == NULL) {
        gtk_entry_buffer_set_text (gtk_entry_get_buffer(GTK_ENTRY (widgets.entryBaudRate)), stringval2, -1);
    }
    
    boolval = g_key_file_get_boolean(key_file, "Settings", "checkTimeStamp", &error);
    if (error == NULL) {
        if(boolval)
            gtk_check_button_set_active (GTK_CHECK_BUTTON(widgets.checkTimeStamp), TRUE);
    }
    
    intval = g_key_file_get_integer (key_file, "Settings", "NewLine", &error);
    if (error == NULL) {
        gtk_drop_down_set_selected (GTK_DROP_DOWN(widgets.comboNewLine), intval);
    }
    g_free(stringval);
    g_free(stringval2);
    g_free(contents);
    g_key_file_free(key_file);
}

void setKeyFile(){
    g_autoptr(GKeyFile) key_file = g_key_file_new ();
    g_autoptr(GError) error = NULL;
    
    const char* entry_textSerialPort = gtk_entry_buffer_get_text (gtk_entry_get_buffer(GTK_ENTRY(widgets.entrySerialPort)));
    g_key_file_set_string (key_file, "Settings", "SerialPort", entry_textSerialPort);
    const char* entry_textBaudRate = gtk_entry_buffer_get_text (gtk_entry_get_buffer(GTK_ENTRY(widgets.entryBaudRate)));
    g_key_file_set_string (key_file, "Settings", "BaudRate", entry_textBaudRate);
    g_key_file_set_boolean(key_file, "Settings", "checkTimeStamp", gtk_check_button_get_active (GTK_CHECK_BUTTON(widgets.checkTimeStamp)));
    g_key_file_set_integer (key_file, "Settings", "NewLine", gtk_drop_down_get_selected(GTK_DROP_DOWN(widgets.comboNewLine)));
    // Save as a file.
    if (!g_key_file_save_to_file (key_file, "settings.ini", &error))
    {
        g_warning ("Error saving key file: %s", error->message);
        //return;
    }
    g_key_file_free(key_file);
}

std::string getFormatedTime(char const *format){
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm ltime;
    localtime_r(&t, &ltime);
    std::ostringstream oss;
    oss << std::put_time(&ltime, format);
    return oss.str();
}
