// up to 4 feeding times, must be in ascending order
#define MAX_FEEDINGS 4
#define DISPENSER_RUN_SECONDS 4
#define DISPLAY_PAGE_DURATION 10
#define DESTICK_MS 500
#define DESTICK_INTERVAL 2

// hour of day
short feeding_times[MAX_FEEDINGS] = {6,18,-1,-1};


enum DISPLAY_PAGE {display_time, display_dispense};
enum MEAL_STATE {meal_waiting, meal_dispensing, meal_dispensed};

