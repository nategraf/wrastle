#include <pebble.h>

#define TRUE 1
#define FALSE 0
#define ACCEL_RATIO 0.05
#define ACCEL_STEP_MS 25
#define PUBLISH_STEP_MS 125
#define STR_SIZE 64
#define COUNT_MAX 24
#define ACCEL_THRESHOLD 900

#define SCORE_BOTT_MARGIN 10
#define SCORE_TOP_MARGIN 0
#define SCORE_SIDE_MARGIN 10
  
#define SCORE_L_KEY 0x65142800
#define SCORE_R_KEY SCORE_L_KEY + 1
#define COUNT_KEY SCORE_R_KEY + 1
#define PATH_KEY COUNT_KEY + 1
  
static Window *s_main_window;
static TextLayer *s_score_layer;
static Layer *s_bar_layer;
static Layer *s_middle_layer;

static char score_str[STR_SIZE];
static int counter;
static bool game_active;
static int score_r;
static int score_l;
static GRect progress_bar;
static GRect middle_box;
static GRect window_frame;
static GRect score_frame;

static void publish_scores(){
  DictionaryIterator *outbox;
  app_message_outbox_begin(&outbox);
  dict_write_cstring(outbox, PATH_KEY, "/scores");
  dict_write_int32(outbox, SCORE_L_KEY, score_l);
  dict_write_int32(outbox, SCORE_R_KEY, score_r);  
  app_message_outbox_send();
}

static void publish_scores_callback(void* data){
  publish_scores();
}

static void publish_count(){
  DictionaryIterator *outbox;
  app_message_outbox_begin(&outbox);
  dict_write_cstring(outbox, PATH_KEY, "/count");
  dict_write_int32(outbox, COUNT_KEY, (counter+COUNT_MAX)*50/COUNT_MAX);
  app_message_outbox_send();
}

static void set_bars(){
  int w = (counter*window_frame.size.w/COUNT_MAX + window_frame.size.w)/2;
  progress_bar.size.w = w;
  layer_mark_dirty(s_bar_layer);
}

static void update_count(AccelData accel){
  if(game_active){
    if(accel.z > ACCEL_THRESHOLD && counter < COUNT_MAX) counter++;
    else if(counter > 0) counter--;
    if(accel.z < -ACCEL_THRESHOLD && counter > -COUNT_MAX) counter--;
    else if(counter < 0) counter++;
  
    publish_count();
    set_bars();
  }
}

static void update_score(){
  if(counter == COUNT_MAX && game_active){
    score_r++;
    vibes_short_pulse();
    game_active = FALSE;
    app_timer_register(PUBLISH_STEP_MS, publish_scores_callback, NULL);
  }
  if(counter == -COUNT_MAX && game_active){
    score_l++;
    vibes_double_pulse();
    game_active = FALSE;
    app_timer_register(PUBLISH_STEP_MS, publish_scores_callback, NULL);
  }
}

static void update_text(){
  snprintf(&score_str[0], STR_SIZE, "%1d-%1d", score_l, score_r);
  text_layer_set_text(s_score_layer, score_str);
} 

static void timer_callback(void *data) {
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);
  
  update_count(accel);
  update_score();
  update_text();
  
  app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void draw_bar(Layer *me, GContext *ctx) {
  #if defined(PBL_COLOR)
    graphics_context_set_fill_color(ctx, GColorRed);
  #endif
  graphics_fill_rect(ctx, progress_bar, 0, GCornerNone);
}

static void draw_middle(Layer *me, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, middle_box, 3, GCornersAll);
}

static void main_window_load(Window *window) {
  // Create and add the layer which displays the progress bar
  Layer *window_layer = window_get_root_layer(window);
  window_frame = layer_get_frame(window_layer);
  
  // Create the progress bar layer
  s_bar_layer = layer_create(window_frame);
  layer_set_update_proc(s_bar_layer, draw_bar);
  layer_add_child(window_layer, s_bar_layer);
  
  // Create the layer for the middle box
  middle_box = GRect(0,0,0,0); // Initialize the box to prevent errors
  s_middle_layer = layer_create(window_frame);
  layer_set_update_proc(s_middle_layer, draw_middle);
  layer_add_child(window_layer, s_middle_layer);
  
  // Create score TextLayer
  s_score_layer = text_layer_create(window_frame);
  text_layer_set_background_color(s_score_layer, GColorClear);
  text_layer_set_text_color(s_score_layer, GColorBlack);
  text_layer_set_text(s_score_layer, "0-0");
  
  // Improve the layout to be more like a watchface
  text_layer_set_font(s_score_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_score_layer, GTextAlignmentCenter);
  
  // Get the size and calculate a frame to center the text vertically
  GSize score_text_size = text_layer_get_content_size(s_score_layer);
  score_frame = window_frame;
  score_frame.origin.y = (window_frame.size.h - score_text_size.h)/2;
  
  // Create the middle box which will be the background for the text
  middle_box.size = score_text_size;
  middle_box.origin.x = (window_frame.size.w - middle_box.size.w)/2 - SCORE_SIDE_MARGIN;
  middle_box.origin.y = (window_frame.size.h - middle_box.size.h)/2 - SCORE_TOP_MARGIN;
  middle_box.size.w += SCORE_SIDE_MARGIN*2;
  middle_box.size.h += SCORE_TOP_MARGIN + SCORE_BOTT_MARGIN;
  
  // Move the score text to its new position
  layer_set_frame(text_layer_get_layer(s_score_layer), score_frame);
  layer_mark_dirty(text_layer_get_layer(s_score_layer));
  layer_mark_dirty(s_middle_layer);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_score_layer));
}

static void main_window_unload(Window *window) {
    // Destroy TextLayer
    text_layer_destroy(s_score_layer);
}

static void game_init(){
  counter = 0;
  score_r = 0;
  score_l = 0;
  game_active = FALSE;
  progress_bar = window_frame;
}

void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  counter = 0;
  game_active = TRUE;
  //app_timer_register(PUBLISH_STEP_MS, publish_timer_callback, NULL);
}

void config_provider(Window *window) {
  window_single_click_subscribe(BUTTON_ID_SELECT, down_single_click_handler);
}

static void init(void) {
  s_main_window = window_create();
  // At compile time, decide whether to use color or not
  #if defined(PBL_COLOR)
    window_set_background_color(s_main_window, GColorBlue);
  #elif defined(PBL_BW)
    window_set_background_color(s_main_window, GColorWhite);
  #endif
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  window_set_click_config_provider(s_main_window, (ClickConfigProvider) config_provider);
  
  accel_data_service_subscribe(0, NULL);
  
  game_init();
  
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void deinit(void) {
  accel_data_service_unsubscribe();

  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}