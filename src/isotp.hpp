#ifndef ISOTPSOCKET_HPP
#define ISOTPSOCKET_HPP

#include <nan.h>
#include <uv.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <iterator>
#include <algorithm>

/* allow PDUs greater 4095 bytes according ISO 15765-2:2015 */
#define MAX_PDU_LENGTH 6000

using namespace v8;

typedef struct
{
  size_t length;
  unsigned char data[MAX_PDU_LENGTH + 1];
} isotp_frame;

class IsoTpSocket : public Nan::ObjectWrap
{
public:
  IsoTpSocket();

  static void Initialize(Local<Object> exports);

private:
  uv_poll_t *poll_;
  int socket_;
  int events_;

  struct can_isotp_options *m_opts;
  struct can_isotp_fc_options *m_fcopts;

  isotp_frame *m_recvBuffer;
  isotp_frame *m_sendBuffer;

  static void New(const Nan::FunctionCallbackInfo<Value> &info);
  static void Bind(const Nan::FunctionCallbackInfo<Value> &info);
  static void Start(const Nan::FunctionCallbackInfo<Value> &info);
  static void Stop(const Nan::FunctionCallbackInfo<Value> &info);
  static void Send(const Nan::FunctionCallbackInfo<Value> &info);

  static void Callback(uv_poll_t *w, int status, int revents);
  void StartInternal();
  void StopInternal();
  void SendInternal(char *data, size_t length);
};

#endif