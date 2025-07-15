#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "cJSON.h"

// cwg
#include "esp_tls.h"
#include "baidu_token.h"


static char *TAG = "baidu_token";


//cwg=20240502
#define MAX_HTTP_OUTPUT_BUFFER              2048
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

char baidu_access_token[100] = "";


//cwg==20240502
/* Event handler for HTTP client events */
esp_err_t token_http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            if (evt->user_data) {
                // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len) {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            } else {
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
            }
            output_len += copy_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}






/* 

// 获取百度语音识别的访问令牌
char *getAccessToken()
{
    char *access_token = NULL;

    esp_http_client_config_t config = {
        .url = "https://aip.baidubce.com/oauth/2.0/token",
        .event_handler = NULL,
        .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 构建请求参数
    char request_params[200];
    snprintf(request_params, sizeof(request_params),
             "grant_type=client_credentials&client_id=%s&client_secret=%s",
             API_KEY, SECRET_KEY);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, request_params, strlen(request_params));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int content_length = esp_http_client_get_content_length(client);
        char *response_buf = malloc(content_length + 1);
        int read_len = esp_http_client_read(client, response_buf, content_length);
        response_buf[read_len] = '\0';

        cJSON *json = cJSON_Parse(response_buf);
        if (json != NULL)
        {
            cJSON *access_token_json = cJSON_GetObjectItem(json, "access_token");
            if (access_token_json != NULL)
            {
                access_token = strdup(access_token_json->valuestring);
            }
            cJSON_Delete(json);
        }

        free(response_buf);
    }

    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "CWG================================access_token: %s\n", access_token);
    return access_token;
}

 */



// 获取百度语音识别的访问令牌
//cwg==20240502
bool refresh_baidu_access_token(const char *client_id, const char *secret_key)
{
    if (client_id == NULL || secret_key == NULL) {
        ESP_LOGE(TAG, "Invalid client information");
        return false;
    }

    char *local_response_buffer = (char *)heap_caps_malloc(MAX_HTTP_OUTPUT_BUFFER * sizeof(char), MALLOC_CAP_SPIRAM);
    if (local_response_buffer == NULL) {
        ESP_LOGE(TAG, "Local response buffer malloc failed");
        return false;
    }

    /* Generate the URL based on the client_id and secret_key */
    char url[256];
    sprintf(url, "https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id=%s&client_secret=%s", client_id, secret_key);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = token_http_event_handler,
        .user_data = local_response_buffer,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if (http_client == NULL) {
        ESP_LOGE(TAG, "HTTP client init failed");
        free(local_response_buffer);
        return false;
    }

    esp_err_t err = esp_http_client_perform(http_client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(http_client),
                esp_http_client_get_content_length(http_client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(http_client);
        free(local_response_buffer);
        return false;
    }

    /* Parse the JSON response */
    cJSON *cjson_root = cJSON_Parse(local_response_buffer);

    strcpy(baidu_access_token, cJSON_GetObjectItem(cjson_root, "access_token")->valuestring);


    ESP_LOGI(TAG, "cwg========================Baidu Access Token: %s", baidu_access_token);

    cJSON_Delete(cjson_root);
    esp_http_client_cleanup(http_client);
    free(local_response_buffer);

    return true;
}


