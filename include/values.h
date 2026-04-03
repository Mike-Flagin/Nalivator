#ifndef NALIVATOR_VALUES_H
#define NALIVATOR_VALUES_H

#define TOUCH_THRESHOLD 30
#define TOUCH_HOLD_TIMEOUT_MS 700

#define MDNS_ADDRESS "nalivator"
#define MDNS_INSTANCE_NAME "Nalivator"
#define HTTP_RESPONSE_BUFF_SIZE 4096

#define LITTLEFS_BASE_PATH "/littlefs"
#define LITTLEFS_MAX_PATH_LENGTH 128

#define SSID_JSON_KEY "ssid"
#define PASSWORD_JSON_KEY "password"

#define PUMPS_JSON_KEY "pumps"
#define FLOWRATE_JSON_KEY "flowrate"
#define INGREDIENT_JSON_KEY "ingredientId"
#define INVERSE_JSON_KEY "inverse"
#define VOLUME_TO_SPLITTER_JSON_KEY "volumeToSplitter"
#define SERVO_POSITIONS_JSON_KEY "servoPositions"
#define VOLUME_AFTER_SPLITTER_JSON_KEY "volumeAfterSplitter"
#define LED_STANDBY_COLOR_JSON_KEY "ledStandbyColor"
#define LED_WAITING_COLOR_JSON_KEY "ledWaitingColor"
#define LED_POURING_COLOR_JSON_KEY "ledPouringColor"
#define LED_POURED_COLOR_JSON_KEY "ledPouredColor"


#endif //NALIVATOR_VALUES_H