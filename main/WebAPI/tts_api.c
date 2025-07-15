/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include"tts_api.h"
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "app_audio.h"
#include "app_ui_ctrl.h"
#include "audio_player.h"
#include "esp_crt_bundle.h"
#include "inttypes.h"



// cwg
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_hal.h"
#include "esp_peripherals.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
//cwg=20240805
#include "bsp_board.h"
#include "bsp/esp-bsp.h"


#define BAIDU_TTS_ENDPOINT "http://tsn.baidu.com/text2audio"                         
#define BAIDU_TTS_TASK_STACK (8*1024)
#define VOICE_ID CONFIG_VOICE_ID
#define VOLUME CONFIG_VOLUME_LEVEL
#define MAX_HTTP_OUTPUT_BUFFER              2048
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

extern char baidu_access_token[100];

typedef struct baidu_tts {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t  i2s_writer;
    audio_element_handle_t  http_stream_reader;
    audio_element_handle_t  mp3_decoder;
    int                     buffer_size;
    char                    *buffer;
    char                    *text;
    int                     sample_rate;
} baidu_tts_t;


static const char *TAG = "TTS-Api";


/* Define a function to handle HTTP events during an HTTP request */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
        file_total_len = 0;
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=(%"PRIu32" + %d) [%d]", file_total_len, evt->data_len, MAX_FILE_SIZE);
        if ((file_total_len + evt->data_len) < MAX_FILE_SIZE) {
            memcpy(audio_rx_buffer + file_total_len, (char *)evt->data, evt->data_len);
            file_total_len += evt->data_len;
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH:%"PRIu32", %"PRIu32" K", file_total_len, file_total_len / 1024);
            
            //cwg注释TTS：从网络端收到所有TTS语音文件后在，在此处进行语音缓存的语音播放。
            FILE *fp = fmemopen((void *)audio_rx_buffer, file_total_len, "rb");

            if (fp) {
                audio_player_play(fp);
                ESP_LOGI(TAG, "cwg===========audio_player_play(fp)============");

            } else{
                //cwg=20240727
                ESP_LOGW(TAG, "cwg==========failed! fp = fmemopen audio_rx_buffer, file_total_len,======================");
            }
        }
        break;


    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}








// //cwg==20240502
/* Event handler for HTTP client events */
esp_err_t tts_token_http_event_handler(esp_http_client_event_t *evt)
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



/* Decode 2 Hex */
char dec2hex(short int c)
{
    if (0 <= c && c <= 9) {
        return c + '0';
    } else if (10 <= c && c <= 15) {
        return c + 'A' - 10;
    } else {
        return -1;
    }
}



/* Encode URL for playing sound */
void url_encode(const char *url, char *encode_out)
{
    int i = 0;
    int len = strlen(url);
    int res_len = 0;

    assert(encode_out);

    for (i = 0; i < len; ++i) {
        char c = url[i];
        char n = url[i + 1];
        if (c == '\\' && n == 'n') {
            i += 1;
            continue;
        } else if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '/' || c == '.') {
            encode_out[res_len++] = c;
        } else {
            int j = (short int)c;
            if (j < 0) {
                j += 256;
            }
            int i1, i0;
            i1 = j / 16;
            i0 = j - i1 * 16;
            encode_out[res_len++] = '%';
            encode_out[res_len++] = dec2hex(i1);
            encode_out[res_len++] = dec2hex(i0);
        }
    }
    encode_out[res_len] = '\0';
}









// //cwg==20240502
// static bool http_refresh_tts_access_token(const char *client_id, const char *secret_key)
// {
//     if (client_id == NULL || secret_key == NULL) {
//         ESP_LOGE(TAG, "Invalid client information");
//         return false;
//     }

//     char *local_response_buffer = (char *)heap_caps_malloc(MAX_HTTP_OUTPUT_BUFFER * sizeof(char), MALLOC_CAP_SPIRAM);
//     if (local_response_buffer == NULL) {
//         ESP_LOGE(TAG, "Local response buffer malloc failed");
//         return false;
//     }

//     /* Generate the URL based on the client_id and secret_key */
//     char url[256];
//     sprintf(url, "https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id=%s&client_secret=%s", client_id, secret_key);

//     esp_http_client_config_t config = {
//         .url = url,
//         .event_handler = tts_token_http_event_handler,
//         .user_data = local_response_buffer,
//         .disable_auto_redirect = true,
//         .crt_bundle_attach = esp_crt_bundle_attach,
//     };

//     esp_http_client_handle_t http_client = esp_http_client_init(&config);
//     if (http_client == NULL) {
//         ESP_LOGE(TAG, "HTTP client init failed");
//         free(local_response_buffer);
//         return false;
//     }

//     esp_err_t err = esp_http_client_perform(http_client);
//     if (err == ESP_OK) {
//         ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
//                 esp_http_client_get_status_code(http_client),
//                 esp_http_client_get_content_length(http_client));
//     } else {
//         ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
//         esp_http_client_cleanup(http_client);
//         free(local_response_buffer);
//         return false;
//     }

//     /* Parse the JSON response */
//     cJSON *cjson_root = cJSON_Parse(local_response_buffer);

//     strcpy(baidu_access_token, cJSON_GetObjectItem(cjson_root, "access_token")->valuestring);


//     ESP_LOGW(TAG, "cwg========================Access token: %s", baidu_access_token);

//     cJSON_Delete(cjson_root);
//     esp_http_client_cleanup(http_client);
//     free(local_response_buffer);

//     return true;
// }










/* Create Text to Speech request */
esp_err_t text_to_speech_request(const char *message, AUDIO_CODECS_FORMAT code_format)
{
    size_t message_len = strlen(message);
    char *encoded_message;
    char *codec_format_str;
    encoded_message = heap_caps_malloc((3 * message_len + 1), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    url_encode(message, encoded_message);

    // if (AUDIO_CODECS_MP3 == code_format) {
    //     codec_format_str = "MP3";
    // } else {
    //     codec_format_str = "WAV";
    // }


    // CWG
    if (AUDIO_CODECS_MP3 == code_format) {
        codec_format_str = "MP3";
    } else {
        codec_format_str = "MP3";
    }


    int url_size = snprintf(NULL, 0, "https://tsn.baidu.com/text2audio?lan=zh&cuid=m2uAfOpV1kUMVG09DgZunNy4tT3MxvVY&ctp=1&aue=3&per=4&tok=%s&tex=%s", \
                            baidu_access_token, \
                            encoded_message);

    // Allocate memory for the URL buffer
    char *url = heap_caps_malloc((url_size + 1), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return ESP_ERR_NO_MEM;
    }

    snprintf(url, url_size + 1, "https://tsn.baidu.com/text2audio?lan=zh&cuid=m2uAfOpV1kUMVG09DgZunNy4tT3MxvVY&ctp=1&aue=3&per=4&tok=%s&tex=%s", \
             baidu_access_token, \
             encoded_message);



    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .buffer_size = 128000,
        .buffer_size_tx = 4000,
        .timeout_ms = 40000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    uint32_t starttime = esp_log_timestamp();
    ESP_LOGI(TAG, "[Start] create_TTS_request, timestamp:%"PRIu32, starttime);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "[End] create_TTS_request, + offset:%"PRIu32, esp_log_timestamp() - starttime);

    heap_caps_free(url);
    heap_caps_free(encoded_message);
    esp_http_client_cleanup(client);
    return err;
}




// ↑↑↑↑↑ audio play代码
// =====================================================================================================================
// =====================================================================================================================
// ↓↓↓↓↓↓ i2s stream代码



static esp_err_t _http_stream_reader_event_handle(http_stream_event_msg_t *msg)
{
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    baidu_tts_t *tts = (baidu_tts_t *)msg->user_data;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // Post text data
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);

        //cwg=20240713
        int payload_len = snprintf(tts->buffer, tts->buffer_size, "lan=zh&cuid=ESP32&ctp=1&vol=15&per=4&tok=%s&tex=%s", baidu_access_token, tts->text);
        esp_http_client_set_post_field(http, tts->buffer, payload_len);
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        esp_http_client_set_header(http, "Content-Type", "application/x-www-form-urlencoded");
        esp_http_client_set_header(http, "Accept", "*/*");


        return ESP_OK;
    }

    //cwg=20240713
    else if (msg->event_id == HTTP_STREAM_ON_RESPONSE) {
        int status_code = esp_http_client_get_status_code(http);
        if (status_code != 200) {
            ESP_LOGE(TAG, "cwg====================[ - ] HTTP request failed, status code = %d", status_code);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}



baidu_tts_handle_t baidu_tts_init(baidu_tts_config_t *config)
{
    // 管道设置
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    baidu_tts_t *tts = calloc(1, sizeof(baidu_tts_t));
    AUDIO_MEM_CHECK(TAG, tts, return NULL);

    tts->pipeline = audio_pipeline_init(&pipeline_cfg);

    tts->buffer_size = config->buffer_size;
    if (tts->buffer_size <= 0) {
        tts->buffer_size = DEFAULT_TTS_BUFFER_SIZE;
    }

    tts->buffer = malloc(tts->buffer_size);
    AUDIO_MEM_CHECK(TAG, tts->buffer, goto exit_tts_init);

    tts->sample_rate = config->playback_sample_rate;

    // I2S流设置
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(0, 16000, 16, AUDIO_STREAM_WRITER);
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    //cwg=20240805
    // i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(0, 16000, 16, AUDIO_STREAM_WRITER);
    // i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    // i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;


    tts->i2s_writer = i2s_stream_init(&i2s_cfg);

    // http流设置
    http_stream_cfg_t http_cfg = {
        .type = AUDIO_STREAM_READER,
        .event_handle = _http_stream_reader_event_handle,
        .user_data = tts,
        .task_stack = BAIDU_TTS_TASK_STACK,
    };
    tts->http_stream_reader = http_stream_init(&http_cfg);

    // MP3流设置
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    tts->mp3_decoder = mp3_decoder_init(&mp3_cfg);

    audio_pipeline_register(tts->pipeline, tts->http_stream_reader, "tts_http");
    audio_pipeline_register(tts->pipeline, tts->mp3_decoder,        "tts_mp3");
    audio_pipeline_register(tts->pipeline, tts->i2s_writer,         "tts_i2s");
    const char *link_tag[3] = {"tts_http", "tts_mp3", "tts_i2s"};
    audio_pipeline_link(tts->pipeline, &link_tag[0], 3);

    // I2S流采样率 位数等设置
    i2s_stream_set_clk(tts->i2s_writer, config->playback_sample_rate, 16, 1);
    // i2s_stream_set_clk(tts->i2s_writer, config->playback_sample_rate, 16, 2);

    return tts;
exit_tts_init:
    baidu_tts_destroy(tts);
    return NULL;
}

esp_err_t baidu_tts_destroy(baidu_tts_handle_t tts)
{
    if (tts == NULL) {
        return ESP_FAIL;
    }
    audio_pipeline_stop(tts->pipeline);
    audio_pipeline_wait_for_stop(tts->pipeline);
    audio_pipeline_terminate(tts->pipeline);
    audio_pipeline_remove_listener(tts->pipeline);
    audio_pipeline_deinit(tts->pipeline);
    free(tts->buffer);
    free(tts);
    return ESP_OK;
}

esp_err_t baidu_tts_set_listener(baidu_tts_handle_t tts, audio_event_iface_handle_t listener)
{
    if (listener) {
        audio_pipeline_set_listener(tts->pipeline, listener);
    }
    return ESP_OK;
}

bool baidu_tts_check_event_finish(baidu_tts_handle_t tts, audio_event_iface_msg_t *msg)
{
    if (msg->source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg->source == (void *) tts->i2s_writer
            && msg->cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg->data == AEL_STATUS_STATE_STOPPED) || ((int)msg->data == AEL_STATUS_STATE_FINISHED))) {
        return true;
    }
    return false;
}


esp_err_t baidu_tts_start(baidu_tts_handle_t tts, const char *text)
{
    free(tts->text);
    tts->text = strdup(text);
    if (tts->text == NULL) {
        ESP_LOGE(TAG, "Error no mem");
        return ESP_ERR_NO_MEM;
    }
    snprintf(tts->buffer, tts->buffer_size, BAIDU_TTS_ENDPOINT);
    audio_pipeline_reset_items_state(tts->pipeline);
    audio_pipeline_reset_ringbuffer(tts->pipeline);
    audio_element_set_uri(tts->http_stream_reader, tts->buffer);
    audio_pipeline_run(tts->pipeline);
    return ESP_OK;
}

esp_err_t baidu_tts_stop(baidu_tts_handle_t tts)
{
    audio_pipeline_stop(tts->pipeline);
    audio_pipeline_wait_for_stop(tts->pipeline);
    ESP_LOGD(TAG, "TTS Stopped");
    return ESP_OK;
}


/* cwg=20240721  tts流逝处理。 Create Text to Speech request */
esp_err_t text_to_speech_request_cwg(const char *message, AUDIO_CODECS_FORMAT code_format)
{
    // cwg=20240721
    ESP_LOGI(TAG, "[ 1 ] Initialize Buttons");
    // Initialize peripherals management
    // esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    //periph_cfg.task_stack = 8*1024;
    // esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    
    //cwg=20240726
    // Initialize Button peripheral
    // periph_button_cfg_t btn_cfg = {
    //     .gpio_mask = (1ULL << get_input_mode_id()) | (1ULL << get_input_rec_id()),
    // };
    // esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);
    // // Start button peripheral
    // esp_periph_start(set, button_handle);


    // 百度 文字转语音 初始化
    baidu_tts_config_t tts_config = {
        .playback_sample_rate = 16000,
    };
    baidu_tts_handle_t tts = baidu_tts_init(&tts_config);

    // 监听“流”
    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from the pipeline");
    baidu_tts_set_listener(tts, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    // audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Listen for all pipeline events");



    ESP_LOGI(TAG, "[ 6 ] cwg======================== tts stream");
    char *answer = strdup(message);
    ESP_LOGI(TAG, "minimax answer = %s", answer);
    // answer_flag = 1;
    // es8311_pa_power(true); // 打开音频
    baidu_tts_start(tts, answer);

    vTaskDelay(pdMS_TO_TICKS(15000));
    ESP_LOGI(TAG, "[ 7 ] cwg tts stream==========pdMS_TO_TICKS(15000)==============DONE");


    // ESP_LOGI(TAG, "[ 6 ] cwg tts stream==========pdMS_TO_TICKS(15000)===============Stop audio_pipeline");
    // // baidu_vtt_destroy(vtt);
    baidu_tts_destroy(tts);

    // /* Stop all periph before removing the listener */
    // esp_periph_set_stop_all(set);
    // audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    // /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    // audio_event_iface_destroy(evt);
    // esp_periph_set_destroy(set);
    // // vTaskDelete(NULL);


    //cwg=20240805
    bsp_codec_set_fs(16000, 16, 2);




/* 
    while (true) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, portMAX_DELAY) != ESP_OK) {
            ESP_LOGW(TAG, "[ * ] Event process failed: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                     msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);
            continue;
        }

        ESP_LOGI(TAG, "[ * ] Event received: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                 msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);

        if (baidu_tts_check_event_finish(tts, &msg)) {
            ESP_LOGI(TAG, "[ * ] cwg tts steram==================TTS Finish");
            // es8311_pa_power(false); // 关闭音频
            continue;
            // break;
        }

        // if (msg.source_type != PERIPH_ID_BUTTON) {
        //     continue;
        // }

        // if ((int)msg.data == get_input_mode_id()) {
        //     break;
        // }

        // if ((int)msg.data != get_input_rec_id()) {
        //     continue;
        // }

        // if (msg.cmd == PERIPH_BUTTON_PRESSED) {
        //     baidu_tts_stop(tts);
        //     ESP_LOGI(TAG, "[ * ] Resuming pipeline");
        //     lcd_clear_flag = 1;
        //     baidu_vtt_start(vtt);
        // } else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE) {
        //     ESP_LOGI(TAG, "[ * ] Stop pipeline");

        //     char *original_text = baidu_vtt_stop(vtt);
        //     if (original_text == NULL) {
        //         minimax_content[0]=0; // 清空minimax 第1个字符写0就可以
        //         continue;
        //     }
        //     ESP_LOGI(TAG, "Original text = %s", original_text);
        //     ask_flag = 1;

        //     char *answer = minimax_chat(original_text);
        //     if (answer == NULL)
        //     {
        //         continue;
        //     }
        //     ESP_LOGI(TAG, "minimax answer = %s", answer);
        //     answer_flag = 1;
        //     es8311_pa_power(true); // 打开音频
        //     baidu_tts_start(tts, answer);
        // }

    } */

    // ESP_LOGI(TAG, "[ 6 ] cwg tts stream==========pdMS_TO_TICKS(15000)===============Stop audio_pipeline");
    // // baidu_vtt_destroy(vtt);
    // baidu_tts_destroy(tts);
    // /* Stop all periph before removing the listener */
    // esp_periph_set_stop_all(set);
    // audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    // /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    // audio_event_iface_destroy(evt);
    // esp_periph_set_destroy(set);
    // // vTaskDelete(NULL);

    // // esp_http_client_handle_t client = esp_http_client_init(&config);
    // // esp_err_t err = esp_http_client_perform(client);
    // // if (err != ESP_OK) {
    // //     ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    // // }

    return ESP_OK;


}









