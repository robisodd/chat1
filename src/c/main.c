#include <pebble.h>
#include "console.h"
static Window *s_window;	
Layer *console_layer;
char text[1000];
bool emulator = false;

enum {
  OPEN_CONNECTION = 0,
  CLOSE_CONNECTION = 1,
  TAP = 3
};

static void error_msg(char *msg) {
  printf("Error: %s", msg);
  console_layer_write_text_attributes(console_layer, msg, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GColorRed, GColorClear, GTextAlignmentCenter, true, true);
}


// ------------------------------------------------------------------------ //
//  PebbleKit JS Functions
// ------------------------------------------------------------------------ //
// Keys for AppMessage Dictionary
// These should correspond to the values you defined in appinfo.json/Settings
enum {
	STATUS_KEY = 0,	
	MESSAGE_KEY = 1,
  USER_KEY = 2,
  SYSTEM_MESSAGE = 3,
  ERR_MESSAGE = 4
};

// Write message to buffer & send
static void send_status(int val) {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_int8(iter, STATUS_KEY, val);
	dict_write_end(iter);
  app_message_outbox_send();
}

static void send_message(char *msg){
	DictionaryIterator *iter;
	
	app_message_outbox_begin(&iter);
	dict_write_cstring(iter, MESSAGE_KEY, msg);
	//dict_write_int8(iter, MESSAGE_KEY, 1);
  
	dict_write_end(iter);
  app_message_outbox_send();
}

// Called when a message is received from PebbleKitJS
static void in_received_handler(DictionaryIterator *received, void *context) {
	Tuple *tuple;
	
	tuple = dict_find(received, STATUS_KEY);
	if(tuple) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "Position: %d", (int)tuple->value->uint32); 
    //snprintf(text, sizeof(text), "Position: %d", (int)tuple->value->uint32);
    //console_layer_write_text(console_layer, text, true);
    
    vibes_cancel(); vibes_enqueue_custom_pattern((VibePattern){.durations = (uint32_t []){30}, .num_segments = 1});  // pulse
	}
	
	tuple = dict_find(received, USER_KEY);
	if(tuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "User: %s", tuple->value->cstring);
    snprintf(text, sizeof(text), "%s:", tuple->value->cstring);
    //console_layer_set_font(console_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    //console_layer_set_text_color(console_layer, GColorBlue);
    //console_layer_write_text(console_layer, text, true);
    console_layer_write_text_attributes(console_layer, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GColorVeryLightBlue, GColorClear, GTextAlignmentCenter, false, true);
    //console_layer_write_text_attributes(console_layer, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GColorBabyBlueEyes, GColorClear, GTextAlignmentCenter, false, true);
	}
  
	tuple = dict_find(received, MESSAGE_KEY);
	if(tuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Message: %s", tuple->value->cstring);
    snprintf(text, sizeof(text), "%s", tuple->value->cstring);
    console_layer_write_text(console_layer, text, true);
	}
  
	tuple = dict_find(received, SYSTEM_MESSAGE);
	if(tuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "System Message: %s", tuple->value->cstring);
    snprintf(text, sizeof(text), "%s", tuple->value->cstring);
    console_layer_write_text_attributes(console_layer, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GColorGreen, GColorClear, GTextAlignmentCenter, false, true);
	}
  
	tuple = dict_find(received, ERR_MESSAGE);
	if(tuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Error Message from JS: %s", tuple->value->cstring);
    snprintf(text, sizeof(text), "%s", tuple->value->cstring);
    console_layer_write_text_attributes(console_layer, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GColorRed, GColorClear, GTextAlignmentCenter, true, true);
	}
}

char *translate_appmessageresult(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

// Called when an incoming message from PebbleKitJS is dropped
static void in_dropped_handler(AppMessageResult reason, void *context) {
  error_msg(translate_appmessageresult(reason));
}

// Called when PebbleKitJS does not acknowledge receipt of a message
static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  error_msg(translate_appmessageresult(reason));
}

// ------------------------------------------------------------------------ //
//  Tapping Functions
// ------------------------------------------------------------------------ //
void send_tap() {
  send_status(TAP);
}
#define SUBTLE_LENGTH        5  // how long (in samples) does it have to be quiet for to mean tap is over
#define SUBTLE_THRESHOLD    12  // how quiet does it need to be (after a tap has started) in order for it to be not-a-tap
#define INITIAL_SENSITIVITY 40  // how hard to hit to start a tap

void accel_data_handler(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
  static int16_t current = 0; // current accelerometer sample.  static so it remembers where the accelerometer was between calls
  static uint8_t gap = 255;   // big gap means tap is not currently happening
  
  for(uint32_t i=0; i<num_samples; i++) {
    int16_t diff = current;                             // backup most recent accelerometer sample
    current = data[i].z >> 3;                           // get new accelerometer sample (all samples seem to be evenly divisible by 8, hence the >>3)
    if(gap==255) {                                      // if this is the first sample taken,
      diff = current;                                   //   there is no difference yet, so set diff to current sample
      gap = 0;                                          //   this stops a tap happening at program start
    }                                                   //   but the accelerometer probably won't be near 0
    diff = diff>current ? diff-current : current-diff;  // Find the delta between the two samples
    
    if(gap<SUBTLE_LENGTH) {                             // tapping still going
      if(diff < SUBTLE_THRESHOLD) {                  // if subtle vibrations (but tapping might still be going)
        gap++;                                         // measure how long it's been subtle for
      } else {           // else: still major vibrations (tap IS still going on)
        gap=0;
      }
    } else {             // else: tapping not currently going
      if(diff >= INITIAL_SENSITIVITY) {       // if major movement, tapping begins!
        send_tap();
        gap = 0;                              // reset gap
      }
    }
  }
}

void init_taps() {
  accel_raw_data_service_subscribe(1, accel_data_handler);  // We will be using the accelerometer: 5 samples_per_update
  accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);    // 100 samples per second = 20 updates per second
}

void deinit_taps() {
  accel_data_service_unsubscribe();
}

// ------------------------------------------------------------------------ //
//  Dictation API Functions
// ------------------------------------------------------------------------ //
#ifdef PBL_SDK_3
static DictationSession *dictation_session = NULL;
static char dictation_text[512];
const char* DictationSessionStatusError[] = {
  "Transcription successful, with a valid result.",
  "User rejected transcription and exited UI.",
  "User exited UI after transcription error.",
  "Too many errors occurred during transcription and the UI exited.",
  "No speech was detected and UI exited.",
  "No BT or internet connection.",
  "Voice transcription disabled for this user.",
  "Voice transcription failed due to internal error.",
  "Cloud recognizer failed to transcribe speech."
};

static void dictation_session_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if(status == DictationSessionStatusSuccess) {
    // Send the dictated text
    snprintf(dictation_text, sizeof(dictation_text), "%s", transcription);
    printf("Sending Dictation Text: %s", dictation_text);
    send_message(dictation_text);
  } else {
    snprintf(dictation_text, sizeof(dictation_text), "Error: %s", DictationSessionStatusError[status]);
    error_msg(dictation_text);
  }
}

static void init_dictation() {
  dictation_session = dictation_session_create(sizeof(dictation_text), dictation_session_callback, NULL);
}

static void deinit_dictation() {
  if(dictation_session)
    dictation_session_destroy(dictation_session);
}
#else
  static void init_dictation() {}
  static void deinit_dictation() {}
  //static void *dictation_session = NULL;
  //void dictation_session_start(void *dictation_session) {}
#endif



// ------------------------------------------------------------------------ //
//  Button Functions
// ------------------------------------------------------------------------ //
void up_click_handler  (ClickRecognizerRef recognizer, void *context) { //   UP   button
  send_status(CLOSE_CONNECTION);
}

void sl_click_handler  (ClickRecognizerRef recognizer, void *context) { // SELECT button
  if(emulator) {
    send_message("Hello Emulator Chaps!");
  } else 
    PBL_IF_MICROPHONE_ELSE(dictation_session_start(dictation_session), error_msg("No Microphone"));
}

void dn_click_handler  (ClickRecognizerRef recognizer, void *context) { //  DOWN  button
  send_status(OPEN_CONNECTION);
}


void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, dn_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, sl_click_handler);
  //window_single_click_subscribe(BUTTON_ID_BACK, bk_click_handler);
}


// ------------------------------------------------------------------------ //
//  Main Functions
// ------------------------------------------------------------------------ //
void main_window_load(Window *window) {
  GRect rect = layer_get_frame(window_get_root_layer(window));
  //GRect rect = GRect(20, 20, 100, 100);

  console_layer = console_layer_create(rect);
  console_layer_set_default_word_wrap(console_layer, true);
  console_layer_set_default_text_color(console_layer, GColorWhite);
  layer_add_child(window_get_root_layer(window), console_layer);
  
  window_set_click_config_provider(window, click_config_provider);
  console_layer_write_text_attributes(console_layer, "Welcome to\nWS Chat 0.2", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GColorInherit, GColorInherit, GTextAlignmentCenter, false, true);
  console_layer_write_text(console_layer, "Up=Disconect Dn=Connect", true);
}

void main_window_unload(Window *window) {
  layer_destroy(console_layer);
}

static void init(void) {
	s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_set_background_color(s_window, GColorBlack);
  
  window_set_click_config_provider(s_window, click_config_provider);
	window_stack_push(s_window, true);
	
	// Register AppMessage handlers
	app_message_register_inbox_received(in_received_handler); 
	app_message_register_inbox_dropped(in_dropped_handler); 
	app_message_register_outbox_failed(out_failed_handler);

  // Initialize AppMessage inbox and outbox buffers with a suitable size
  const int inbox_size = 1024;
  const int outbox_size = 1024;
	app_message_open(inbox_size, outbox_size);
  
  // Create new dictation session
  init_dictation();
  
  init_taps();
}

static void deinit(void) {
  deinit_taps();
  deinit_dictation();
	app_message_deregister_callbacks();
	window_destroy(s_window);
}

int main( void ) {
  emulator = watch_info_get_model()==WATCH_INFO_MODEL_UNKNOWN;
  if(emulator) {
    light_enable(true);  // Good colors on emulator
    printf("Emulator Detected: Turning Backlight On");
  }

	init();
	app_event_loop();
	deinit();
}