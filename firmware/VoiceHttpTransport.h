#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <new>

// The UI owns the audio buffer. It must not record into it while busy().
// Only the main loop consumes replies and changes device/UI state.
class VoiceHttpTransport {
public:
  struct Reply { uint32_t generation=0; int status=0; String body; };
  bool busy() const { return active_; }
  bool start(const String& url, const String& mode, const uint8_t* audio,
             size_t bytes, uint32_t generation) {
    if (active_ || !audio || !bytes) return false;
    if (!replies_) replies_=xQueueCreate(1,sizeof(Reply*));
    if (!replies_) return false;
    Job* job=new(std::nothrow) Job;
    if (!job) return false;
    job->reply=new(std::nothrow) Reply;
    if (!job->reply) { delete job; return false; }
    job->reply->generation=generation;
    job->url=url; job->mode=mode; job->audio=audio; job->bytes=bytes; job->queue=replies_;
    active_=true;
    if (xTaskCreate(run,"voice-http",8192,job,1,nullptr)!=pdPASS) {
      active_=false; delete job->reply; delete job; return false;
    }
    return true;
  }
  bool poll(Reply& reply) {
    Reply* received=nullptr;
    if (!replies_ || xQueueReceive(replies_,&received,0)!=pdTRUE) return false;
    reply=*received; delete received; active_=false; return true;
  }
private:
  struct Job { String url,mode; const uint8_t* audio; size_t bytes; QueueHandle_t queue; Reply* reply; };
  QueueHandle_t replies_=nullptr;
  bool active_=false;
  static void run(void* argument) {
    Job* job=static_cast<Job*>(argument);
    HTTPClient http;
    http.setConnectTimeout(2500); http.setTimeout(90000);
    if (http.begin(job->url)) {
      http.addHeader("Content-Type","application/octet-stream");
      http.addHeader("X-Voice-Mode",job->mode);
      job->reply->status=http.POST(const_cast<uint8_t*>(job->audio),job->bytes);
      // Read incrementally: a bridge failure cannot allocate an unbounded String.
      if (job->reply->status==200) {
        auto* stream=http.getStreamPtr();
        const int expected=http.getSize();
        uint32_t lastByte=millis();
        while (stream && (http.connected() || stream->available()) &&
               (expected<0 || static_cast<int>(job->reply->body.length())<expected)) {
          while (stream->available()) {
            if (job->reply->body.length()>=8192) { job->reply->status=-2; break; }
            job->reply->body+=static_cast<char>(stream->read()); lastByte=millis();
          }
          if (job->reply->status!=200 || millis()-lastByte>5000) break;
          vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (expected>=0 && static_cast<int>(job->reply->body.length())!=expected) job->reply->status=-3;
      }
      http.end();
    } else job->reply->status=-1;
    Reply* reply=job->reply;
    xQueueSend(job->queue,&reply,portMAX_DELAY);
    delete job;
    vTaskDelete(nullptr);
  }
};
