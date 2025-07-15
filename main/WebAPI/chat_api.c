#include "chat_api.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "cJSON.h"

static char *TAG = "chat_api";

const char *url = "https://api.xiaoyukefu.com/v1/chat/completions";
const char *apiKey = "sk-proj-7T2YH3TxodJ35qW9uFg5T3BlbkFJggVDQqhEBo6qaxxxxxx"; // 替换为您的OpenAI API密钥

static char response_data[4096 * 2];


// 聊天历史记录队列
#define MAX_CHAT_HISTORY 10
static char *chat_history[MAX_CHAT_HISTORY];
static int chat_history_length = 0;


// http客户端的事件处理回调函数
static esp_err_t http_client_event_handler1(esp_http_client_event_t *evt)
{
    static int recived_len;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_CONNECTED:
        recived_len = 0;
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->user_data)
        {
            memcpy(evt->user_data + recived_len, evt->data, evt->data_len); // 将分片的每一片数据都复制到user_data
            recived_len += evt->data_len;                                   // 累计偏移更新
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        recived_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        recived_len = 0;
        break;
    case HTTP_EVENT_ERROR:
        recived_len = 0;
        break;
    default:
        break;
    }

    return ESP_OK;
}

char *getGPTAnswer(char *prompt)
{
    char *answer = NULL;

    // 添加新的聊天记录到聊天历史队列
    if (chat_history_length >= MAX_CHAT_HISTORY)
    {
        free(chat_history[0]); // 释放最早的聊天记录内存
        for (int i = 0; i < chat_history_length - 1; i++)
        {
            chat_history[i] = chat_history[i + 1]; // 向前移动聊天记录指针
        }
        chat_history_length--;
    }
    chat_history[chat_history_length] = strdup(prompt);
    chat_history_length++;

    // 构建请求参数
    cJSON *root = cJSON_CreateObject();
    cJSON *messages_array = cJSON_CreateArray();

    // 添加之前的聊天历史记录
    for (int i = 0; i < chat_history_length; i++)
    {
        cJSON *message_item = cJSON_CreateObject();
        cJSON_AddStringToObject(message_item, "role", "user");
        cJSON_AddStringToObject(message_item, "content", chat_history[i]);
        cJSON_AddItemToArray(messages_array, message_item);
    }

    // 添加用户输入的消息
    cJSON *user_message = cJSON_CreateObject();
    cJSON_AddStringToObject(user_message, "role", "user");
    cJSON_AddStringToObject(user_message, "content", prompt);
    cJSON_AddItemToArray(messages_array, user_message);

    cJSON_AddStringToObject(root, "model", "gpt-3.5-turbo");
    cJSON_AddItemToObject(root, "messages", messages_array);

    cJSON_AddNumberToObject(root, "temperature", 1);
    cJSON_AddNumberToObject(root, "presence_penalty", 0);
    cJSON_AddNumberToObject(root, "frequency_penalty", 0);

    char *request_params = cJSON_PrintUnformatted(root);
    ESP_LOGE(TAG, "Chat request: %s\n", request_params);
    cJSON_Delete(root);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_client_event_handler1,
        .user_data = response_data,
        .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    // esp_http_client_set_header(client, "Authorization", apiKey);
    // cwg http.addHeader("Authorization", "Bearer " + String(API_KEY));
    char tempHeader[40];
    sprintf(tempHeader, "Bearer %s", apiKey);
    esp_http_client_set_header(client, "Authorization", tempHeader);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, request_params, strlen(request_params));
    // 设置超时时间20s
    esp_http_client_set_timeout_ms(client, 20000);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {

        /* I (18632) chat_api: cwg====================Chat Response Data: {
          "id": "chatcmpl-9GoatS7pET1VK0PdUu2Ai0tMBm6ur",
          "object": "chat.completion",
          "created": 1713794759,
          "model": "gpt-3.5-turbo-0125",
          "choices": [
            {
              "index": 0,
              "message": {
                "role": "assistant",
                "content": "���我���是���您���的���虚���拟���助���手���，���可���以���帮�� �助���您���回���答���问���题���和���提���供���信���息���。���有���什���么�� �我���可���以���帮���助���您���的���吗���？"
              },
              "logprobs": null,
              "finish_reason": "stop"
            }
          ],
          "usage": {
            "prompt_tokens": 25,
            "completion_tokens": 42,
            "total_tokens": 67
          },
          "system_fingerprint": "fp_c2295e73ad"
        } */

        ESP_LOGI(TAG, "cwg====================Chat Response Data: %s", response_data);

        cJSON *json = cJSON_Parse(response_data);


        if (json != NULL)
        {
            cJSON *choices_array = cJSON_GetObjectItem(json, "choices");
            if (choices_array != NULL && cJSON_IsArray(choices_array) && cJSON_GetArraySize(choices_array) > 0)
            {
                cJSON *message_obj = cJSON_GetObjectItem(cJSON_GetArrayItem(choices_array, 0), "message");
                if (message_obj != NULL && cJSON_IsObject(message_obj))
                {
                    // answer = strdup(cJSON_GetObjectItem(message_obj, "content")->valuestring);
                    // cwg
                    ESP_LOGI(TAG, "cwg20240422~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~message obj true\n");
                    cJSON *content_item = cJSON_GetObjectItem(message_obj, "content");

                    if (content_item != NULL && cJSON_IsString(content_item))
                    {
                        ESP_LOGI(TAG, "cwg20240422~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~content_item is string.\n");
                        answer = strdup(content_item->valuestring);
                    
                    }
                    else
                    {

                        char *message_str = cJSON_Print(message_obj);
                        ESP_LOGI(TAG, "cwg20240422~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~message_str===\n %s", message_str);

                        if (strlen(message_str) > 37) {
                            ESP_LOGI(TAG, "cwg20240422~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~content_start true.\n");
                            // content_start += strlen(key);  // 移动指针到内容的开始位置。
                            // 为 content_str 分配内存空间，需要的大小为剩余字符串的长度加1（为了'\0'终止符）
                            char *content_str = (char *)malloc(strlen(message_str) - 36);
                            // 检查内存是否成功分配
                            if (content_str != NULL) {
                                strcpy(content_str, message_str + 37); // 复制从第31个字符开始的所有字符
                                ESP_LOGI(TAG, "cwg20240422~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Content: %s\n", content_str);  // 打印新的内容字符串
                                // 使用完后释放内存
                                answer = malloc(strlen(content_str) + 1);
                                strcpy(answer, content_str);
                                free(content_str);
                            } else {
                                ESP_LOGW(TAG, "Memory allocation failed.\n");
                            }
                        } else {
                            ESP_LOGE(TAG, "大模型API访问失败, Content value is NULL or not a string");
                            answer = strdup("大模型API访问失败");
                        }

                    }
                    
                }
            }
            cJSON_Delete(json);
        }

        
    }

    else
    {
        answer = "Chat HTTP Post failed";
    }

    free(request_params);
    esp_http_client_cleanup(client);
    return answer;
}