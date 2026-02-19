#define USER_SETUP_INFO "ESP32-2432S028"

// ---- DRIVER ----
//#define ILI9341_DRIVER
#define ST7789_DRIVER

// ---- COLOR ORDER ----
#define TFT_RGB_ORDER TFT_BGR
//#define TFT_INVERSION_ON
// ak by boli farby prehodené → TFT_BGR

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---- SPI PINS ----
//#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// ---- BACKLIGHT ----
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// ---- TOUCH ----
#define TOUCH_CS 33


//#define T_MISO 12
#define T_MOSI 13
#define T_SCLK 14


#define SPI_TOUCH_FREQUENCY 1000000

// ---- SPI SPEED ----
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

#define LOAD_GLCD  // Font 1. Štandardný malý font (vyžaduje sa pre TextSize 1 a 2)
#define LOAD_FONT2 // Font 2. Stredný font
#define LOAD_FONT4 // Font 4. Väčší font
