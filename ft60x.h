#pragma once

#define FTDI_VENDOR_ID               0x0403
#define FTDI_FT60X_PRODUCT_ID        0x601f
#define FTDI_COMMUNICATION_INTERFACE 0x00
#define FTDI_DATA_INTERFACE          0x01
#define FTDI_ENDPOINT_SESSION_OUT    0x01
#define FTDI_ENDPOINT_OUT            0x02
#define FTDI_ENDPOINT_IN             0x82

typedef struct __attribute__ ((packed)) {
  /*
   * Device descriptor.
   */
  uint16_t vendor_id;
  uint16_t product_id;

  /*
   * String descriptors.
   */
  char     string_descriptors[128];

  /*
   * Configuration descriptor.
   */
  uint8_t     reserved;
  uint8_t     power_attributes;
  uint16_t    power_consumption;

  /*
   * Data transfer configuration.
   */
  uint8_t     reserved_2;
  uint8_t     fifo_clock;
  uint8_t     fifo_mode;
  uint8_t     channel_config;

  /*
   * Optional feature support.
   */
  uint16_t    optional_feature_support;
  uint8_t     battery_charging_gpio_config;
  uint8_t     ro_flash_eeprom_detection;
  
  /*
   * MSIO and GPIO Configuration.
   */
  uint32_t    msio_control;
  uint32_t    gpio_control;
} ft60x_config;

/*
 * Chip configuration - FIFO Mode.
 */
typedef enum {
  CONFIGURATION_FIFO_MODE_245,
  CONFIGURATION_FIFO_MODE_600,
  CONFIGURATION_FIFO_MODE_COUNT,
} ft60x_fifo_mode;

/*
 * Chip configuration - Channel configuration.
 */
typedef enum {
  CONFIGURATION_CHANNEL_CONFIG_4,
  CONFIGURATION_CHANNEL_CONFIG_2,
  CONFIGURATION_CHANNEL_CONFIG_1,
  CONFIGURATION_CHANNEL_CONFIG_1_OUT_PIPE,
  CONFIGURATION_CHANNEL_CONFIG_1_IN_PIPE,
  CONFIGURATION_CHANNEL_CONFIG_COUNT,
} ft60x_channel_config;

/*
 * Chip configuration - Optional feature support.
 */
typedef enum {
  CONFIGURATION_OPTIONAL_FEATURE_DISABLE_ALL = 0,
  CONFIGURATION_OPTIONAL_FEATURE_ENABLE_BATTERY_CHARGING = 1,
  CONFIGURATION_OPTIONAL_FEATURE_DISABLE_CANCEL_SESSION_UNDERRUN = 2,
  CONFIGURATION_OPTIONAL_FEATURE_ENABLE_NOTIFICATION_MESSAGE_IN_CH_1 = 4,
  CONFIGURATION_OPTIONAL_FEATURE_ENABLE_NOTIFICATION_MESSAGE_IN_CH_2 = 8,
  CONFIGURATION_OPTIONAL_FEATURE_ENABLE_NOTIFICATION_MESSAGE_IN_CH_3 = 0x10,
  CONFIGURATION_OPTIONAL_FEATURE_ENABLE_NOTIFICATION_MESSAGE_IN_CH_4 = 0x20,
  CONFIGURATION_OPTIONAL_FEATURE_ENABLE_NOTIFICATION_MESSAGE_IN_CH_ALL = 0x3C,
  CONFIGURATION_OPTIONAL_FEATURE_DISABLE_UNDERRUN_IN_CH_1   = (0x1 << 6),
  CONFIGURATION_OPTIONAL_FEATURE_DISABLE_UNDERRUN_IN_CH_2   = (0x1 << 7),
  CONFIGURATION_OPTIONAL_FEATURE_DISABLE_UNDERRUN_IN_CH_3   = (0x1 << 8),
  CONFIGURATION_OPTIONAL_FEATURE_DISABLE_UNDERRUN_IN_CH_4   = (0x1 << 9),
  CONFIGURATION_OPTIONAL_FEATURE_DISABLE_UNDERRUN_IN_CH_ALL = (0xF << 6),
} ft60x_opt_features;

typedef struct  __attribute__ ((packed)) {
  uint32_t idx;
  uint8_t  pipe;
  uint8_t  cmd;
  uint8_t  unk1;
  uint8_t  unk2;
  uint32_t len;
  uint32_t unk4;
  uint32_t unk5;
} ft60x_ctrl_req;
