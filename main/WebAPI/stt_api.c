#include "stt_api.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "cJSON.h"

static char *TAG = "stt_api";



//cwg=20240502
#define MAX_HTTP_OUTPUT_BUFFER              2048
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
extern char baidu_access_token[100];


static char response_data[2500];
static int recived_len;



// http客户端的事件处理回调函数
static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "connected to web-server");
        recived_len = 0;
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->user_data)
        {
            // cwg检查缓冲区溢出
            if (recived_len + evt->data_len > sizeof(response_data) - 1) {
                ESP_LOGE(TAG, "cwg=============Response buffer overflow detected!");
                return ESP_FAIL; // 或者你可以选择处理这种情况，而不是直接返回失败
            }
            
            memcpy(evt->user_data + recived_len, evt->data, evt->data_len); // 将分片的每一片数据都复制到user_data
            recived_len += evt->data_len;                                   // 累计偏移更新
        
            // cwg日志记录接收到的数据长度
            ESP_LOGI(TAG, "cwg===============Received data length: %d, Total length: %d", evt->data_len, recived_len);

        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "finished a request and response!");
        recived_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "disconnected to web-server");
        recived_len = 0;
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "error");
        recived_len = 0;
        break;
    default:
        break;
    }

    return ESP_OK;
}








//cwg==20240502
/* Event handler for HTTP client events */
// esp_err_t token_http_event_handler(esp_http_client_event_t *evt)
// {
//     static char *output_buffer;  // Buffer to store response of http request from event handler
//     static int output_len;       // Stores number of bytes read
//     switch(evt->event_id) {
//         case HTTP_EVENT_ERROR:
//             ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
//             break;
//         case HTTP_EVENT_ON_CONNECTED:
//             ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
//             break;
//         case HTTP_EVENT_HEADER_SENT:
//             ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
//             break;
//         case HTTP_EVENT_ON_HEADER:
//             ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
//             break;
//         case HTTP_EVENT_ON_DATA:
//             ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
//             /*
//              *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
//              *  However, event handler can also be used in case chunked encoding is used.
//              */
//             // If user_data buffer is configured, copy the response into the buffer
//             int copy_len = 0;
//             if (evt->user_data) {
//                 // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
//                 copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
//                 if (copy_len) {
//                     memcpy(evt->user_data + output_len, evt->data, copy_len);
//                 }
//             } else {
//             if (!esp_http_client_is_chunked_response(evt->client)) {
//                 int content_len = esp_http_client_get_content_length(evt->client);
//                     if (output_buffer == NULL) {
//                         // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
//                         output_buffer = (char *) calloc(content_len + 1, sizeof(char));
//                         output_len = 0;
//                         if (output_buffer == NULL) {
//                             ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
//                             return ESP_FAIL;
//                         }
//                     }
//                     copy_len = MIN(evt->data_len, (content_len - output_len));
//                     if (copy_len) {
//                         memcpy(output_buffer + output_len, evt->data, copy_len);
//                     }
//                 }
//             }
//             output_len += copy_len;
//             break;
//         case HTTP_EVENT_ON_FINISH:
//             ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
//             if (output_buffer != NULL) {
//                 // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
//                 // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
//                 free(output_buffer);
//                 output_buffer = NULL;
//             }
//             output_len = 0;
//             break;
//         case HTTP_EVENT_DISCONNECTED:
//             ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
//             int mbedtls_err = 0;
//             esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
//             if (err != 0) {
//                 ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
//                 ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
//             }
//             if (output_buffer != NULL) {
//                 free(output_buffer);
//                 output_buffer = NULL;
//             }
//             output_len = 0;
//             break;
//         case HTTP_EVENT_REDIRECT:
//             ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
//             esp_http_client_set_header(evt->client, "From", "user@example.com");
//             esp_http_client_set_header(evt->client, "Accept", "text/html");
//             esp_http_client_set_redirection(evt->client);
//             break;
//     }
//     return ESP_OK;
// }





char *baidu_asr(uint8_t *audio_data, int audio_len)
{
    char *asr_data = NULL;
    char url[256]; // Define a buffer to hold the URL

    // Define the parameters
    char dev_pid[] = "1537";   // 普通话识别
    char cuid[] = "mOV4XcqL848kBHXu9kcNisVOWcq2DNgN";  // ID

    
    // Construct the URL dynamically
    sprintf(url, "http://vop.baidu.com/server_api?dev_pid=%s&cuid=%s&token=%s", dev_pid, cuid, baidu_access_token);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_client_event_handler,
        .user_data = response_data};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, (const char *)audio_data, audio_len);
    esp_http_client_set_header(client, "Content-Type", "audio/wav;rate=16000");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        cJSON *json = cJSON_Parse(response_data);

        if (json != NULL)
        {
            cJSON *result_json = cJSON_GetObjectItem(json, "result");
            if (result_json != NULL && cJSON_IsArray(result_json))
            {
                cJSON *result_array = cJSON_GetArrayItem(result_json, 0);
                if (result_array != NULL && cJSON_IsString(result_array))
                {
                    asr_data = strdup(result_array->valuestring);
                }
            }
            cJSON_Delete(json);
        }

        ESP_LOGE(TAG, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~result_data: %s\n", asr_data);
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    return asr_data;
}
