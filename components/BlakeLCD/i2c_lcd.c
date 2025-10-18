#include "i2c_lcd.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "unistd.h"

static const char *TAG = "LCD";
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t lcd_dev = NULL;

i2c_master_bus_config_t bus_conf = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .clk_source = I2C_CLK_SRC_DEFAULT,
};

esp_err_t i2c_master_init(uint8_t lcd_addr)
{
    if (i2c_bus) return ESP_OK; // already initialized

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_conf, &i2c_bus));

    // Add LCD as device on this bus
    i2c_device_config_t dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = lcd_addr,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_conf, &lcd_dev));

    ESP_LOGI(TAG, "I2C bus + LCD device initialized (addr=0x%02x)", lcd_addr);
    return ESP_OK;
}

esp_err_t add_lcd_device(i2c_master_bus_handle_t existing_i2c_bus, uint8_t lcd_addr){
    i2c_device_config_t dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = lcd_addr,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(existing_i2c_bus, &dev_conf, &lcd_dev));
    return ESP_OK;
}

static esp_err_t lcd_write(uint8_t *data, size_t len)
{
    return i2c_master_transmit(lcd_dev, data, len, 1000);
}

void lcd_send_cmd(char cmd)
{
    uint8_t data_t[4];
    uint8_t data_u = (cmd & 0xf0);
    uint8_t data_l = ((cmd << 4) & 0xf0);

    data_t[0] = data_u | 0x0C;
    data_t[1] = data_u | 0x08;
    data_t[2] = data_l | 0x0C;
    data_t[3] = data_l | 0x08;

    if (lcd_write(data_t, 4) != ESP_OK)
        ESP_LOGE(TAG, "Error sending command");
}
void lcd_send_data(char data)
{
    char data_u, data_l;
    uint8_t data_t[4];
    
    data_u = (data & 0xf0); // Upper nibble of the data
    data_l = ((data << 4) & 0xf0); // Lower nibble of the data
    
    data_t[0] = data_u | 0x0D; // Enable (EN) = 1, Register Select (RS) = 1
    data_t[1] = data_u | 0x09; // Enable (EN) = 0, Register Select (RS) = 1
    data_t[2] = data_l | 0x0D; // Enable (EN) = 1, Register Select (RS) = 1
    data_t[3] = data_l | 0x09; // Enable (EN) = 0, Register Select (RS) = 1
    
    // Write data to the I2C device
    if (lcd_write(data_t, 4) != ESP_OK)
        ESP_LOGE(TAG, "Error Sending Data");
}

void lcd_clear(void)
{
    lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY); // Clear display command
    usleep(5000); // Wait for the command to execute
}

void lcd_put_cursor(int row, int col)
{
    switch (row)
    {
        case 0:
            col |= LCD_CMD_SET_CURSOR; // Set position for row 0
            break;
        case 1:
            col |= (LCD_CMD_SET_CURSOR | 0x40); // Set position for row 1
            break;
    }

    lcd_send_cmd(col); // Send command to set cursor position
}

void lcd_init(void)
{
    // NEW:   i2c must be initialized first
    //    i2c_master_init(); // Initialize I2C master interface
    // 4-bit initialization sequence
    usleep(50000); // Wait for >40ms
    lcd_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    usleep(5000);  // Wait for >4.1ms
    lcd_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    usleep(200);  // Wait for >100us
    lcd_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    usleep(10000);
    lcd_send_cmd(LCD_CMD_INIT_4_BIT_MODE);  // Set 4-bit mode
    usleep(10000);

    // Display initialization
    lcd_send_cmd(LCD_CMD_FUNCTION_SET); // Function set: 4-bit mode, 2-line display, 5x8 characters
    usleep(1000);
    lcd_send_cmd(LCD_CMD_DISPLAY_OFF); // Display off
    usleep(1000);
    lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY);  // Clear display
    usleep(1000);
    lcd_send_cmd(LCD_CMD_ENTRY_MODE_SET); // Entry mode set: increment cursor, no shift
    usleep(1000);
    lcd_send_cmd(LCD_CMD_DISPLAY_ON); // Display on, cursor off, blink off
    usleep(1000);
}

void lcd_send_string(char *str)
{
    while (*str) lcd_send_data(*str++); // Send each character of the string
}
