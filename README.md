




1.使用audio play的tts代码，可以正常唤醒，对话。
2.使用i2s stream的tts代码，无法播放tts也无法唤醒设备。
3.设备功能：唤醒-说话-在线ASR-chat llm—在线TTS。





======================================================

1. 硬件设备：esp32s3-box3
2. idf版本：5.1.0
3. adf版本（tts中用到）：adf-master版本

4. 项目网络与第三方API权限配置：
(1)设备烧录后，默认的wifi网络名称与密码已经设置。
   可打开手机热点，设置wifi网络名称为00DEMO，设置wifi密码为demo00520。
   这样设备烧录后即可联网。
(2)百度ASR与TTS的API访问权限：将如下代码位置的key改为自己的key
   D:\Program_Files\esp-box-master\examples\chatgpt_wgdemo\main\WebAPI\baidu_token.h
   #define API_KEY "xxxx"
   #define SECRET_KEY "xxxx"
(3)chatgpt的镜像访问链接以及key设置：将如下代码位置的连接以及key改为自己的key
   const char *url = "https://api.xiaoyukefu.com/v1/chat/completions";
   const char *apiKey = "sk-proj-xxxx"; // 替换为您的OpenAI API密钥
   目前代码尚未优化，此处其他通过menuconfig以及tinyuf2进行修改无效。

5. 代码烧录：
(1)该项目不需要下载esp-box项目，可独立编译。
(2)烧录顺序：
   cd D:\Program_Files\esp-box-master\examples\chatgpt_wgdemo\factory_nvs
   可选：idf.py set-target esp32s3
   可选：idf.py menuconfig
   必须：idf.py builid flash

   cd D:\Program_Files\esp-box-master\examples\chatgpt_wgdemo
   可选：idf.py set-target esp32s3
   可选：idf.py menuconfig
   必须：idf.py builid flash

6. 烧录成功后运行，在网络成功并且baidu_access_token刷新后，可通过唤醒词唤醒设备，然后进行对话。
   每次对话均需先唤醒设备。





