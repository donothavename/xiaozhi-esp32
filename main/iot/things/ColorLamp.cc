#include "iot/thing.h"
#include <esp_log.h>

#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <atomic>
#include <mutex>
#include <cmath>

#define TAG "ColorLamp"

namespace iot
{

    // 这里仅定义 ColorLamp 的属性和方法，不包含具体的实现
    class ColorLamp : public Thing
    {
    private:
        std::mutex mutex_;
        long ColorLamp_counter_ = 0;
        long ColorLamp_change_ms_ = 1500;
        TaskHandle_t ColorLamp_task_ = nullptr;
        led_strip_handle_t led_strip_ = nullptr;
        esp_timer_handle_t ColorLamp_timer_ = nullptr;
        uint8_t r = 0, g = 0, b = 0;
        uint8_t style_ = 0, speed_ = 5;

        void InitializeGpio(gpio_num_t gpio)
        {
            assert(gpio != GPIO_NUM_NC);

            led_strip_config_t strip_config = {};
            strip_config.strip_gpio_num = gpio;
            strip_config.max_leds = 1;
            strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
            strip_config.led_model = LED_MODEL_WS2812;

            led_strip_rmt_config_t rmt_config = {};
            rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz

            ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
            led_strip_clear(led_strip_);

            esp_timer_create_args_t timer_args = {
                .callback = [](void *arg)
                {
                    auto lamp = static_cast<ColorLamp *>(arg);
                    std::lock_guard<std::mutex> lock(lamp->mutex_);
                    float progress = 1.0 * (lamp->ColorLamp_counter_ += lamp->speed_) / lamp->ColorLamp_change_ms_;
                    if (progress > 1.0)
                    {
                        lamp->ColorLamp_counter_ = 0;
                        progress = 1.0;
                    }
                    switch (lamp->style_)
                    {
                    case 0:
                        led_strip_clear(lamp->led_strip_);
                        break;
                    case 1:
                        lamp->rainbowAnimation(progress);
                        break;
                    case 2:
                        lamp->breathingLightAnimation(progress);
                        break;
                    }
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "ColorLamp_timer",
                .skip_unhandled_events = false,
            };
            ESP_ERROR_CHECK(esp_timer_create(&timer_args, &ColorLamp_timer_));
            ESP_ERROR_CHECK(esp_timer_start_periodic(ColorLamp_timer_, 10000));
        }

        void HsvToRgb(float h, float s, float v)
        {
            int i = static_cast<int>(h * 6);
            float f = h * 6 - i;
            float p = v * (1 - s);
            float q = v * (1 - f * s);
            float t = v * (1 - (1 - f) * s);

            switch (i % 6)
            {
            case 0:
                r = v * 255;
                g = t * 255;
                b = p * 255;
                break;
            case 1:
                r = q * 255;
                g = v * 255;
                b = p * 255;
                break;
            case 2:
                r = p * 255;
                g = v * 255;
                b = t * 255;
                break;
            case 3:
                r = p * 255;
                g = q * 255;
                b = v * 255;
                break;
            case 4:
                r = t * 255;
                g = p * 255;
                b = v * 255;
                break;
            case 5:
                r = v * 255;
                g = p * 255;
                b = q * 255;
                break;
            }
        }

        void rainbowAnimation(float progress)
        {
            HsvToRgb(progress, 1.0f, 0.05f);
            led_strip_set_pixel(led_strip_, 0, r, g, b);
            led_strip_refresh(led_strip_);
        }

        void breathingLightAnimation(float progress)
        {
            if (progress < 0.01)
            {
                float hue;
                hue = esp_random() % 360 / 360.0f;
                HsvToRgb(hue, 1.0f, 0.5);
            }
            float brightness;
            brightness = pow((progress < 0.5 ? progress : (1.0f - progress)) * 2.0f, 2.5) * 0.4;
            led_strip_set_pixel(led_strip_, 0, r * brightness, g * brightness, b * brightness);
            led_strip_refresh(led_strip_);
        }

    public:
        ColorLamp() : Thing("ColorLamp", "机器人的彩灯,在用户需要放歌时AI可以主动开启")
        {
            InitializeGpio(GPIO_NUM_48);
            methods_.AddMethod("SetSpeed", "设置彩灯变化速度", ParameterList({
                                                                   Parameter("speed", "0-9的整数，0关闭，1最慢，9最快", kValueTypeNumber, true),
                                                               }),
                               [this](const ParameterList &parameters)
                               {
                                   speed_ = parameters["speed"].number() + 2;
                               });
            methods_.AddMethod("SetStyle", "设置彩灯样式", ParameterList({Parameter("style", "返回0-2,对应样式为1:单色变换,2:呼吸变换,0:沉寂熄灭(关闭)", kValueTypeNumber, true)}),
                               [this](const ParameterList &parameters)
                               {
                                   style_ = parameters["style"].number();
                               });
        }
    };
} // namespace iot
DECLARE_THING(ColorLamp);
