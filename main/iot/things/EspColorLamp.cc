#include "iot/thing.h"
#include <esp_log.h>
#include "esp_netif.h"
#include <esp_http_client.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "EspColorLamp"

namespace iot
{

    // 这里仅定义 EspColorLamp 的属性和方法，不包含具体的实现
    class EspColorLamp : public Thing
    {

    public:
        EspColorLamp() : Thing("EspColorLamp", "Esp彩虹球")
        {
            methods_.AddMethod("SetStyle", "设置彩虹球样式", ParameterList({Parameter("style", "返回A-I或S,对应样式为A:流光溢彩,B:烟花绽放,C:彩虹层替,D:旋转彩虹,E:单色变换,F:色彩闪烁,G:红白圣诞,H:彩虹漩涡,I:呼吸变换,S:沉寂熄灭(关闭)", kValueTypeString, true)}), [this](const ParameterList &parameters)
                               {
            std::string style = parameters["style"].string();
            //SendCommand(style);
            xTaskCreate(SendCommand, "http_task", 4096, new std::string(style), 5, NULL); });
            methods_.AddMethod("SetSpeed", "设置彩虹球变化速度", ParameterList({Parameter("speed", "0-9的整数，0关闭，1最快，9最慢", kValueTypeString, true)}), [this](const ParameterList &parameters)
                               {
            std::string speed = parameters["speed"].string();
            //SendCommand(speed);
            xTaskCreate(SendCommand, "http_task", 4096, new std::string(speed), 5, NULL); });
        }

    private:
        static std::string get_ip_prefix_by_bitwise(void)
        {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif == NULL)
            {
                // ESP_LOGE(TAG, "获取网络接口失败");
                return std::string("");
            }

            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
            {
                uint32_t ip = ip_info.ip.addr;

                // 提取前三个段
                int ip1 = (ip >> 0) & 0xFF;
                int ip2 = (ip >> 8) & 0xFF;
                int ip3 = (ip >> 16) & 0xFF;

                // ESP_LOGI(TAG, "前三段 IP: %d.%d.%d", ip1, ip2, ip3);
                return std::to_string(ip1) + "." + std::to_string(ip2) + "." + std::to_string(ip3);
            }
            else
            {
                // ESP_LOGE(TAG, "获取 IP 信息失败");
                return std::string("");
            }
        }

        static void SendCommand(void *param)
        {
            std::string value = *(std::string *)param;
            delete (std::string *)param;
            std::string ip_prefix = get_ip_prefix_by_bitwise();
            if (ip_prefix.empty())
            {
                ESP_LOGE(TAG, "IP 前缀为空，无法发送命令");
                return;
            }
            std::string url = "http://" + ip_prefix + ".177/command";

            esp_http_client_config_t config = {
                .url = url.c_str(),
                .method = HTTP_METHOD_GET,
            };
            esp_http_client_handle_t client = esp_http_client_init(&config);

            std::string query = "?input=" + value;
            ESP_LOGI(TAG, "URL: %s%s", url.c_str(), query.c_str());
            esp_http_client_set_url(client, (url.c_str() + query).c_str());

            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                         esp_http_client_get_status_code(client),
                         esp_http_client_get_content_length(client));
            }
            else
            {
                ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
            }
            esp_http_client_cleanup(client);
            vTaskDelete(NULL);
        }
    };

} // namespace iot

DECLARE_THING(EspColorLamp);