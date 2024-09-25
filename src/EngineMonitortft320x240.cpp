#define INCLUDE_TFT

// Generic Arduino
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <lv_tc.h>
#include <lv_tc_screen.h>
#include <esp_nvs_tc.h>
#include <Wifi.h>
#include "EngineMonitortft320x240.h"

#if defined ( INCLUDE_TFT )

// Instantiate display
TFT_eSPI tft = TFT_eSPI();

// forward declarations
void hdrDisplay(void);
void do_lvgl_init(uint32_t );
static void setStyle();
static void buildStatusBar();
static void buildBody();
static void buildRPMGuage();
static void buildOilGuage();
static void buildTempGuage();
static void updateMainScreen(lv_timer_t *);
static void updateMainScreen_test(lv_timer_t *);
static void updateStatusBar(lv_timer_t *);

// lvgl stuff, main screen defines
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];
static lv_style_t label_style;

// some constants for various display elements
#define MS_HIEGHT (tft.height() - 37) // allow for a little room at the sides
#define MS_WIDTH (tft.width() - 2) 
#define STATUS_BAR_HIEGHT 30

/* lvgl Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();

    lv_disp_flush_ready( disp_drv );
} // end my_disp_flush

/* Read the touchpad */
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;

    bool touched = tft.getTouch( &touchX, &touchY, 600 );

    if( !touched )
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;
    }
} // end my_touchpad_read

// used by tc calibrate, just restart after tc is calibrated
void your_tc_finish_cb(
        lv_event_t *event
    ) {
    // Calibration finished so just restart application
    // could also run the application here but easier to just restart
    // which cleans up the cal screen etc.
    ESP.restart();
} // end your_tc_finish_cb

static lv_style_t border_style;
static lv_style_t smallBorder_style;
static lv_style_t style_btn;
static lv_style_t body_style;
static lv_style_t indicator_style;
static lv_style_t smallIndicator_style;
static lv_style_t statusBarText_style;
static lv_style_t vertBarStyle;
static lv_style_t vertBarStyleIndic;
static lv_obj_t *wifiLabel;
static lv_obj_t *ipLabel;
static lv_obj_t *ssidLabel;
static lv_timer_t *updateMainScreenTimer;
static lv_timer_t *updateMainScreen_testTimer;
static lv_timer_t *updateStatusBarTimer;
static lv_obj_t *bodyScreen;
static lv_obj_t *engineRpmGauge;
static lv_meter_indicator_t *engineRpmIndic;
static lv_obj_t *engineOilGauge;
static lv_meter_indicator_t *engineOilIndic;
static lv_obj_t *engineRPMIndicator;
static lv_obj_t *engineRPMText;
static lv_obj_t *engineOilIndicator;
static lv_obj_t *engineOilText;
static lv_obj_t *tempGuageObject;
static lv_obj_t *tempGuageBar;
static lv_obj_t *tempGuageBar_shadow;
static lv_obj_t *tempGuageIndicator;
static lv_obj_t *tempGuageText;
static lv_obj_t *tempGuageBarTextLower;
static lv_obj_t *tempGuageBarTextMid;
static lv_obj_t *tempGuageBarTextUpper;
static lv_obj_t *battGuageObject;
static lv_obj_t *battGuageBar;
static lv_obj_t *battGuageBar_shadow;
static lv_obj_t *battGuageIndicator;
static lv_obj_t *battGuageText;
static lv_obj_t *battGuageBarTextLower;
static lv_obj_t *battGuageBarTextMid;
static lv_obj_t *battGuageBarTextUpper;


// LVGL Basic Example Code
static void event_cb(lv_event_t * e)
{
    LV_LOG_USER("Clicked");

    static uint32_t cnt = 1;
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    lv_label_set_text_fmt(label, "%" LV_PRIu32, cnt);
    cnt++;
}

/**
 * Add click event to a button
 */
void lv_example_event_1(void)
{
    lv_obj_t * btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 100, 50);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Click me!");
    lv_obj_center(label);
}
//#endif

// init lvgl graphics
void do_lvgl_init(uint32_t rot){

    String LVGL_Arduino = "LVGL: ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println( LVGL_Arduino );
    
    // init the screen hw
    tft.begin();          /* TFT init */
    tft.setRotation( rot ); /* 3 = landscape/flipped 1 = landscape/no flip*/
 
    // init graphics lib
    lv_init();
    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    /*Initialize the (dummy) input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    
    lv_tc_indev_drv_init(&indev_drv, my_touchpad_read);
    lv_indev_drv_register( &indev_drv );

   // this will force a recalibration by erasing the nvs data
   #if defined (ERASE_TC_CAL)
        // always erase cal data - remove later
    esp_nvs_tc_coeff_erase();
   #endif

    // Calibration code
    /*
        If using NVS:
        Register a calibration coefficients save callback.
    */
    
    // register function to save cal data
    lv_tc_register_coeff_save_cb(esp_nvs_tc_coeff_save_cb);

    // if cal data exists then we are good to go, otherwise cal the calibration routine 
    if(esp_nvs_tc_coeff_init()) {
        /*
            Data exists: proceed with the normal application without
            showing the calibration screen
        */
        // start the main UI code here
        setStyle();
        buildStatusBar();
        buildBody();
        updateMainScreenTimer = lv_timer_create(updateMainScreen, 1000, NULL);
        updateMainScreen_testTimer = lv_timer_create(updateMainScreen_test, 2000, NULL);
        updateStatusBarTimer = lv_timer_create(updateStatusBar, 2500, NULL);
      } else {
        /*
            There is no cal data: load the calibration screen, perform the calibration, then restart
        */
        // tc cal screen
        lv_obj_t *tCScreen = lv_tc_screen_create();
        // call back for tc cal finished
        lv_obj_add_event_cb(tCScreen, your_tc_finish_cb, LV_EVENT_READY, NULL);

        // do the tc cal, when complete we get the LV_EVENT_READY event for the tCScreen object
        lv_disp_load_scr(tCScreen);
        lv_tc_screen_start(tCScreen);
    } // end if
} // end do_lvgl_init

// setup all the styles for this project
static void setStyle() {
  
  // overall style 
  lv_style_init(&border_style);
  lv_style_set_border_width(&border_style, 2);
  lv_style_set_border_color(&border_style, lv_color_black());
  lv_style_set_text_font(&border_style, &lv_font_montserrat_12);

  // body style 
  lv_style_init(&body_style);
  lv_style_set_border_width(&body_style, 2);
  lv_style_set_border_color(&body_style, lv_color_black());
  lv_style_set_text_font(&body_style, &lv_font_montserrat_10);

  // small text version 
  lv_style_init(&smallBorder_style);
  lv_style_set_border_width(&smallBorder_style, 2);
  lv_style_set_border_color(&smallBorder_style, lv_color_black());
  lv_style_set_text_font(&smallBorder_style, &lv_font_montserrat_8);
  
  // style for indicator readouts
  lv_style_init(&indicator_style);
  lv_style_set_border_width(&indicator_style, 1);
  lv_style_set_radius(&indicator_style,2);
  lv_style_set_border_color(&indicator_style, lv_palette_main(LV_PALETTE_GREY));
  lv_style_set_text_font(&indicator_style, &lv_font_montserrat_20);

  // style for indicator readouts
  lv_style_init(&smallIndicator_style);
  lv_style_set_border_width(&smallIndicator_style, 1);
  lv_style_set_radius(&smallIndicator_style,2);
  lv_style_set_border_color(&smallIndicator_style, lv_palette_main(LV_PALETTE_GREY));
  lv_style_set_text_font(&indicator_style, &lv_font_montserrat_10);

  // status bar, wifi text (changes color with connectivity)
  lv_style_init(&style_btn);
  lv_style_set_bg_color(&style_btn, lv_color_hex(0xC5C5C5));
  lv_style_set_bg_opa(&style_btn, LV_OPA_50);
  lv_style_set_text_color(&style_btn, lv_color_make(255, 0, 0));

  // status bar, non wifi text, color is static
  lv_style_init(&statusBarText_style);
  lv_style_set_text_color(&style_btn, lv_color_make(0, 0, 0));

  lv_style_init(&vertBarStyle);
  lv_style_set_border_color(&vertBarStyle, lv_palette_main(LV_PALETTE_BLUE_GREY));
  lv_style_set_border_width(&vertBarStyle, 2);
  lv_style_set_pad_all(&vertBarStyle, 2); /*To make the indicator smaller*/
  lv_style_set_radius(&vertBarStyle, 1);
  lv_style_set_anim_time(&vertBarStyle, 1000);

  lv_style_init(&vertBarStyleIndic);
  lv_style_set_bg_opa(&vertBarStyleIndic, LV_OPA_COVER);
  lv_style_set_bg_color(&vertBarStyleIndic, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_radius(&vertBarStyleIndic, 0);

} // end setstyle

// generic status bar for project
static void buildStatusBar() {

  // header bar at top of screen
  lv_obj_t *statusBar = lv_obj_create(lv_scr_act());
  lv_obj_set_size(statusBar, tft.width()-4, STATUS_BAR_HIEGHT);
  lv_obj_align(statusBar, LV_ALIGN_TOP_MID, 1, 2);

  lv_obj_add_style(statusBar, &style_btn, 0);
  lv_obj_remove_style(statusBar, NULL, LV_PART_SCROLLBAR | LV_STATE_ANY);

  // wifi indicator
  wifiLabel = lv_label_create(statusBar);
  lv_obj_set_size(wifiLabel, 50, STATUS_BAR_HIEGHT);
  lv_label_set_text(wifiLabel, "WiFi " LV_SYMBOL_CLOSE);
  lv_obj_align(wifiLabel, LV_ALIGN_LEFT_MID, 4, 8);

  // ip address if connected
  ipLabel = lv_label_create(statusBar);
  lv_obj_set_size(ipLabel, 100, STATUS_BAR_HIEGHT);
  lv_label_set_text(ipLabel, " ");
  lv_obj_align(ipLabel, LV_ALIGN_LEFT_MID, 58, 8);

  // ssid if connected
  ssidLabel = lv_label_create(statusBar);
  lv_obj_set_size(ssidLabel, 180, STATUS_BAR_HIEGHT);
  lv_label_set_text(ssidLabel, "SSID: ");
  lv_obj_align(ssidLabel, LV_ALIGN_LEFT_MID, 160, 8);

} // end buildstatusbar

// helper function to update the main display including header bar
static void updateStatusBar(lv_timer_t *timer)
{
  char buff[255];
  // update wifi symbol
  if (WiFi.status() == WL_CONNECTED) {
      lv_style_set_text_color(&style_btn, lv_color_make(0, 0, 0));
      lv_label_set_text(wifiLabel, "WiFi " LV_SYMBOL_WIFI);
      // ip address
      sprintf(buff,"IP: %s", WiFi.localIP().toString());
      lv_label_set_text(ipLabel, buff);
      lv_obj_align(ipLabel, LV_ALIGN_LEFT_MID, 58, 8);
      // ssid 
      sprintf(buff,"SSID: %s", WiFi.SSID());
      lv_label_set_text(ssidLabel, buff);
      lv_obj_align(ssidLabel, LV_ALIGN_LEFT_MID, 160, 8);

  } else {
      lv_style_set_text_color(&style_btn, lv_color_make(255, 0, 0));
      lv_label_set_text(wifiLabel, "WiFi " LV_SYMBOL_CLOSE);
      // ip address
      lv_label_set_text(ipLabel, "IP: ");
      lv_obj_align(ipLabel, LV_ALIGN_LEFT_MID, 58, 8);
      // ssid
      sprintf(buff,"SSID: ");
      lv_label_set_text(ssidLabel, buff);
      lv_obj_align(ssidLabel, LV_ALIGN_LEFT_MID, 160, 8);

  } // end if
} // end updateStatusBar

// helper to update the main display including header bar
static void updateMainScreen(lv_timer_t *timer)
{
    // needle is rpm/100 since guage is in 100's of rpm
    // grab a current copy from datatable
    lv_meter_set_indicator_value(engineRpmGauge, engineRpmIndic, (uint32_t)(EngRPM/100));
    // display engine rpm
    lv_label_set_text_fmt(engineRPMIndicator, " %04d ", (uint32_t)(EngRPM));
    
    // needle is oil pressure
    uint32_t locOilP = (uint32_t)(OilPres/6894.75);
    lv_meter_set_indicator_value(engineOilGauge, engineOilIndic, locOilP);
    // display oil pressure psi
    lv_label_set_text_fmt(engineOilIndicator, " %03d ", locOilP);

    // Engine temp
    // (296K − 273.15) × 9/5 + 32
    int32_t locEngTemp = (int32_t)((((engineCoolantTemp-273.15)*(9.0/5.0))+32.0));
    if (tempGuageBar_shadow != NULL) {
      lv_bar_set_value(tempGuageBar_shadow, locEngTemp, LV_ANIM_ON);
      lv_obj_invalidate(tempGuageBar_shadow);
    }
    lv_label_set_text_fmt(tempGuageIndicator, " %03d ", locEngTemp);

    // Alternator Voltage
    int32_t locEngVoltage = (int32_t)(AltVolts * 10);
    if (locEngVoltage <= 90)
      locEngVoltage = 90;
    if (battGuageBar_shadow != NULL) {
      lv_bar_set_value(battGuageBar_shadow, locEngVoltage, LV_ANIM_ON);
      lv_obj_invalidate(battGuageBar_shadow);
    }
    lv_label_set_text_fmt(battGuageIndicator, " %04.1f ", (float)(locEngVoltage/10.0));
} // end updateMainScreen

// helper on timer, not used for now
static void updateMainScreen_test(lv_timer_t *timer)
{
  // leave empty routine for now, might be good for debugging
} // end setvalue

// build main screen
static void buildBody() {
  // main screen frame
  bodyScreen = lv_obj_create(lv_scr_act());
  lv_obj_add_style(bodyScreen, &border_style, 0);
  lv_obj_set_size(bodyScreen, MS_WIDTH, MS_HIEGHT);
  lv_obj_align(bodyScreen, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_scrollbar_mode(bodyScreen, LV_SCROLLBAR_MODE_OFF);

  // engin rpm guage
  buildRPMGuage();
  buildOilGuage();
  buildTempGuage();

} // end buildbody

// temperature guage stuff
#define TEMP_GUAGE_HIEGHT 110
#define TEMP_GUAGE_WIDTH 55
#define ENGINE_TEMP_GUAGE_XOFFSET 100
#define ENGINE_TEMP_GUAGE_YOFFSET 50
#define TEMP_GUAGE_YOFFSET_TXT 10

// tempGuageBar event handler
static void tempGuageBar_event_cb(lv_event_t * e)
{
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if(dsc->part != LV_PART_INDICATOR) return;

    lv_obj_t * obj = lv_event_get_target(e);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    //label_dsc.font = &lv_font_montserrat_10;
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", (int)lv_bar_get_value(tempGuageBar));

    lv_point_t txt_size;
    lv_txt_get_size(&txt_size, buf, label_dsc.font, label_dsc.letter_space, label_dsc.line_space, LV_COORD_MAX,
                    label_dsc.flag);

    lv_area_t txt_area;
    /*If the indicator is long enough put the text inside on the right*/
    if(lv_area_get_width(dsc->draw_area) > txt_size.x + 20) {
        txt_area.x2 = dsc->draw_area->x2 - 5;
        txt_area.x1 = txt_area.x2 - txt_size.x + 1;
        label_dsc.color = lv_color_white();
    }
    /*If the indicator is still short put the text out of it on the right*/
    else {
        txt_area.x1 = dsc->draw_area->x2 + 5;
        txt_area.x2 = txt_area.x1 + txt_size.x - 1;
        label_dsc.color = lv_color_black();
    }

    txt_area.y1 = dsc->draw_area->y1 + (lv_area_get_height(dsc->draw_area) - txt_size.y) / 2;
    txt_area.y2 = txt_area.y1 + txt_size.y - 1;

    lv_draw_label(dsc->draw_ctx, &label_dsc, &txt_area, buf, NULL);
}// end tempGuageBar_event_cb


// build the vertical guages on the main screen
static void buildTempGuage() {

  // start with base object, turn on border. Everything is within object so can be moved
  tempGuageObject = lv_obj_create(bodyScreen);
  lv_obj_align_to(tempGuageObject, bodyScreen, LV_ALIGN_CENTER, ENGINE_TEMP_GUAGE_XOFFSET, ENGINE_TEMP_GUAGE_YOFFSET);
  lv_obj_set_size(tempGuageObject, TEMP_GUAGE_WIDTH, TEMP_GUAGE_HIEGHT);
  lv_obj_add_style(tempGuageObject, &body_style, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_scrollbar_mode(tempGuageObject, LV_SCROLLBAR_MODE_OFF);

  // add animated bar
  lv_obj_t * tempGuageBar = lv_bar_create(tempGuageObject);
  tempGuageBar_shadow = tempGuageBar; // work around since tempGuageBar is getting cloberred

  lv_obj_remove_style_all(tempGuageBar);  /*To have a clean start*/
  lv_obj_add_style(tempGuageBar, &vertBarStyle, 0);
  lv_obj_add_style(tempGuageBar, &vertBarStyleIndic, LV_PART_INDICATOR);
  
  lv_bar_set_range(tempGuageBar, 0, 150);
  //lv_obj_add_event_cb(tempGuageBar, tempGuageBar_event_cb, LV_EVENT_DRAW_PART_END, NULL);
  lv_obj_set_size(tempGuageBar, 15, TEMP_GUAGE_HIEGHT-30);
  lv_obj_align_to(tempGuageBar, tempGuageObject, LV_ALIGN_BOTTOM_MID, 10, -10);
  lv_bar_set_value(tempGuageBar, (int32_t)(76/*getOwTempValues(3)*/), LV_ANIM_ON);

  /* some static text on the display */
  tempGuageText = lv_label_create(tempGuageObject);
  lv_obj_align_to(tempGuageText, tempGuageObject, LV_ALIGN_BOTTOM_MID, 22, TEMP_GUAGE_YOFFSET_TXT-1);
  lv_label_set_text(tempGuageText,"°F");

  /* display the rpm in numerical format as well */
  tempGuageIndicator = lv_label_create(tempGuageObject);
  lv_obj_add_style(tempGuageIndicator, &indicator_style, 0);
  lv_obj_set_style_text_font(tempGuageIndicator, &lv_font_montserrat_12, 0);
  lv_obj_align_to(tempGuageIndicator, tempGuageObject, LV_ALIGN_BOTTOM_MID, -9, TEMP_GUAGE_YOFFSET_TXT);
  lv_label_set_text_fmt(tempGuageIndicator, " %03d ", 0);

  /* meter scale */
  tempGuageBarTextLower = lv_label_create(tempGuageObject);
  lv_obj_set_style_text_font(tempGuageBarTextLower, &lv_font_montserrat_10, 0);
  lv_obj_align_to(tempGuageBarTextLower, tempGuageObject, LV_ALIGN_BOTTOM_MID, -4, -10);
  lv_label_set_text(tempGuageBarTextLower,"0");
  
  tempGuageBarTextMid = lv_label_create(tempGuageObject);
  lv_obj_set_style_text_font(tempGuageBarTextMid, &lv_font_montserrat_10, 0);
  lv_obj_align_to(tempGuageBarTextMid, tempGuageObject, LV_ALIGN_BOTTOM_MID, -7, -45);
  lv_label_set_text(tempGuageBarTextMid,"075");

  tempGuageBarTextUpper = lv_label_create(tempGuageObject);
  lv_obj_set_style_text_font(tempGuageBarTextUpper, &lv_font_montserrat_10, 0);
  lv_obj_align_to(tempGuageBarTextUpper, tempGuageObject, LV_ALIGN_BOTTOM_MID, -7, -80);
  lv_label_set_text(tempGuageBarTextUpper,"150");


  // start with base object, turn on border. Everything is within object so can be moved
  battGuageObject = lv_obj_create(bodyScreen);
  lv_obj_align_to(battGuageObject, bodyScreen, LV_ALIGN_CENTER, ENGINE_TEMP_GUAGE_XOFFSET + TEMP_GUAGE_WIDTH + 7, ENGINE_TEMP_GUAGE_YOFFSET);
  lv_obj_set_size(battGuageObject, TEMP_GUAGE_WIDTH, TEMP_GUAGE_HIEGHT);
  lv_obj_add_style(battGuageObject, &smallBorder_style, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_scrollbar_mode(battGuageObject, LV_SCROLLBAR_MODE_OFF);

  // add animated bar
  lv_obj_t * battGuageBar = lv_bar_create(battGuageObject);
  battGuageBar_shadow = battGuageBar; // work around since battGuageBar is getting cloberred
  lv_obj_remove_style_all(battGuageBar);  /*To have a clean start*/
  lv_obj_add_style(battGuageBar, &vertBarStyle, 0);
  lv_obj_add_style(battGuageBar, &vertBarStyleIndic, LV_PART_INDICATOR);

  lv_bar_set_range(battGuageBar, 90, 160);
  //lv_obj_add_event_cb(battGuageBar, battGuageBar_event_cb, LV_EVENT_DRAW_PART_END, NULL);
  lv_obj_set_size(battGuageBar, 15, TEMP_GUAGE_HIEGHT-30);
  lv_obj_align_to(battGuageBar, battGuageObject, LV_ALIGN_BOTTOM_MID, 10, -10);
  lv_bar_set_value(battGuageBar, 105, LV_ANIM_ON);

  /* some static text on the display */
  battGuageText = lv_label_create(battGuageObject);
  lv_obj_align_to(battGuageText, battGuageObject, LV_ALIGN_BOTTOM_MID, 26, TEMP_GUAGE_YOFFSET_TXT-1);
  lv_label_set_text(battGuageText,"V");

  /* display the rpm in numerical format as well */
  battGuageIndicator = lv_label_create(battGuageObject);
  lv_obj_add_style(battGuageIndicator, &indicator_style, 0);
  lv_obj_set_style_text_font(battGuageIndicator, &lv_font_montserrat_12, 0);
  lv_obj_align_to(battGuageIndicator, battGuageObject, LV_ALIGN_BOTTOM_MID, -9, TEMP_GUAGE_YOFFSET_TXT);
  lv_label_set_text_fmt(battGuageIndicator, " %04.1f ", 0);

  
  /* meter scale */
  battGuageBarTextLower = lv_label_create(battGuageObject);
  lv_obj_set_style_text_font(battGuageBarTextLower, &lv_font_montserrat_10, 0);
  lv_obj_align_to(battGuageBarTextLower, battGuageObject, LV_ALIGN_BOTTOM_MID, -5, -10);
  lv_label_set_text(battGuageBarTextLower,"9.0");
  
  battGuageBarTextMid = lv_label_create(battGuageObject);
  lv_obj_set_style_text_font(battGuageBarTextMid, &lv_font_montserrat_10, 0);
  lv_obj_align_to(battGuageBarTextMid, battGuageObject, LV_ALIGN_BOTTOM_MID, -7, -45);
  lv_label_set_text(battGuageBarTextMid,"12.5");

  battGuageBarTextUpper = lv_label_create(battGuageObject);
  lv_obj_set_style_text_font(battGuageBarTextUpper, &lv_font_montserrat_10, 0);
  lv_obj_align_to(battGuageBarTextUpper, battGuageObject, LV_ALIGN_BOTTOM_MID, -7, -80);
  lv_label_set_text(battGuageBarTextUpper,"16.0");

} // end buildTempGuage

// locate engine rpm guage on the main screen relative to centre of bodyscreen object
#define RPM_GUAGE_HIEGHT 170
#define RPM_GUAGE_WIDTH 170
#define ENGINE_RPM_GUAGE_XOFFSET -80
#define ENGINE_RPM_GUAGE_YOFFSET -20

// build the rpm guage on the main screen
static void buildRPMGuage() {
  // create and rpm guage on main display
  engineRpmGauge = lv_meter_create(bodyScreen);
  // this removes the outline, allows for more usable space
  lv_obj_remove_style(engineRpmGauge, NULL, LV_PART_MAIN);
  // Locate and set size of rpm guage
  lv_obj_align_to(engineRpmGauge, bodyScreen, LV_ALIGN_CENTER, ENGINE_RPM_GUAGE_XOFFSET, ENGINE_RPM_GUAGE_YOFFSET);
  lv_obj_set_size(engineRpmGauge, RPM_GUAGE_WIDTH, RPM_GUAGE_HIEGHT);
  

  /*Add a scale first*/
  lv_meter_scale_t * scale = lv_meter_add_scale(engineRpmGauge);
  lv_meter_set_scale_range(engineRpmGauge, scale, 0, 50, 270, 135);
  lv_meter_set_scale_ticks(engineRpmGauge, scale, 51, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(engineRpmGauge, scale, 10, 4, 14, lv_color_black(), 10);
  
  /* Green range - arc */
  engineRpmIndic = lv_meter_add_arc(engineRpmGauge, scale, 3, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_meter_set_indicator_start_value(engineRpmGauge, engineRpmIndic, 0);
  lv_meter_set_indicator_end_value(engineRpmGauge, engineRpmIndic, 35);

  /* Green range - ticks */
  engineRpmIndic = lv_meter_add_scale_lines(engineRpmGauge, scale, lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_GREEN),false, 0);
  lv_meter_set_indicator_start_value(engineRpmGauge, engineRpmIndic, 0);
  lv_meter_set_indicator_end_value(engineRpmGauge, engineRpmIndic, 35);

  /* Red range - arc */
  engineRpmIndic = lv_meter_add_arc(engineRpmGauge, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
  lv_meter_set_indicator_start_value(engineRpmGauge, engineRpmIndic, 35);
  lv_meter_set_indicator_end_value(engineRpmGauge, engineRpmIndic, 50);

  /* Red range - arc */
  engineRpmIndic = lv_meter_add_scale_lines(engineRpmGauge, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false,0);
  lv_meter_set_indicator_start_value(engineRpmGauge, engineRpmIndic, 35);
  lv_meter_set_indicator_end_value(engineRpmGauge, engineRpmIndic, 50);

  /* Add the indicator needle and set initial value*/
  engineRpmIndic = lv_meter_add_needle_line(engineRpmGauge, scale, 4, lv_palette_main(LV_PALETTE_BLUE_GREY), -17);
  lv_meter_set_indicator_value(engineRpmGauge, engineRpmIndic, 0);

  /* some static text on the display */
  engineRPMText = lv_label_create(engineRpmGauge);
  lv_obj_align_to(engineRPMText, engineRpmGauge, LV_ALIGN_CENTER, -20, 15);
  lv_label_set_text(engineRPMText,"RPM x100");

  /* display the rpm in numerical format as well */
  engineRPMIndicator = lv_label_create(engineRpmGauge);
  lv_obj_add_style(engineRPMIndicator, &indicator_style, 0);
  lv_obj_set_align(engineRPMIndicator, LV_ALIGN_BOTTOM_MID);
  lv_obj_set_style_text_font(engineRPMIndicator, &lv_font_montserrat_20, 0);
  lv_label_set_text_fmt(engineRPMIndicator, " %04d ", 0);
} // end buildrpmguage

// locate engine oil guage on the main screen relative to centre of bodyscreen object
#define ENGINE_OIL_GUAGE_HIEGHT 85
#define ENGINE_OIL_GUAGE_WIDTH 85
#define ENGINE_OIL_GUAGE_XOFFSET 100
#define ENGINE_OIL_GUAGE_YOFFSET -53

// build the oil pressure on the main screen
static void buildOilGuage() {
  // create and rpm guage on main display
  engineOilGauge = lv_meter_create(bodyScreen);
  lv_obj_add_style(engineOilGauge, &smallBorder_style, 0);
  lv_obj_remove_style(engineOilGauge, NULL, LV_PART_MAIN);

  // set the size
  lv_obj_set_size(engineOilGauge, ENGINE_OIL_GUAGE_WIDTH, ENGINE_OIL_GUAGE_HIEGHT);
  lv_obj_align_to(engineOilGauge, bodyScreen, LV_ALIGN_CENTER, ENGINE_OIL_GUAGE_XOFFSET, ENGINE_OIL_GUAGE_YOFFSET);
  
  /*Add a scale first*/
  lv_meter_scale_t * scale = lv_meter_add_scale(engineOilGauge);
  lv_meter_set_scale_range(engineOilGauge, scale, 0, 80, 180, 180);
  lv_meter_set_scale_ticks(engineOilGauge, scale, 5, 0, 0, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(engineOilGauge, scale, 1, 2, 6, lv_color_black(), 9);
  
  /* Green range - arc */
  engineOilIndic = lv_meter_add_arc(engineOilGauge, scale, 1, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_meter_set_indicator_start_value(engineOilGauge, engineOilIndic, 20);
  lv_meter_set_indicator_end_value(engineOilGauge, engineOilIndic, 80);

  /* Green range - ticks */
  engineOilIndic = lv_meter_add_scale_lines(engineOilGauge, scale, lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_GREEN),false, 0);
  lv_meter_set_indicator_start_value(engineOilGauge, engineRpmIndic, 20);
  lv_meter_set_indicator_end_value(engineOilGauge, engineRpmIndic, 80);

  /* Red range - arc */
  engineOilIndic = lv_meter_add_arc(engineOilGauge, scale, 1, lv_palette_main(LV_PALETTE_RED), 0);
  lv_meter_set_indicator_start_value(engineOilGauge, engineOilIndic, 0);
  lv_meter_set_indicator_end_value(engineOilGauge, engineOilIndic, 20);

  /* Red range - arc */
  engineOilIndic = lv_meter_add_scale_lines(engineOilGauge, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false,0);
  lv_meter_set_indicator_start_value(engineOilGauge, engineOilIndic, 0);
  lv_meter_set_indicator_end_value(engineOilGauge, engineOilIndic, 20);

  /* Add the indicator needle and set initial value*/
  engineOilIndic = lv_meter_add_needle_line(engineOilGauge, scale, 2, lv_palette_main(LV_PALETTE_BLUE_GREY), -17);
  lv_meter_set_indicator_value(engineOilGauge, engineOilIndic, 0);

  /* some static text on the display */
  engineOilText = lv_label_create(engineOilGauge);
  lv_obj_align_to(engineOilText, engineOilGauge, LV_ALIGN_CENTER, 20, 16);
  lv_label_set_text(engineOilText,"Psi");

  /* display the rpm in numerical format as well */
  engineOilIndicator = lv_label_create(engineOilGauge);
  lv_obj_add_style(engineOilIndicator, &indicator_style, 0);
  lv_obj_align_to(engineOilIndicator, engineOilGauge, LV_ALIGN_CENTER, -14, 15);
  lv_obj_set_style_text_font(engineOilIndicator, &lv_font_montserrat_12, 0);
  lv_label_set_text_fmt(engineOilIndicator, " %03d ", 0);
} // end buildoilguage

#endif