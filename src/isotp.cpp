#include "isotp.hpp"

using namespace v8;

Nan::Persistent<Function> constructor;

IsoTpSocket::IsoTpSocket(v8::Local<v8::Function> onError,
                         v8::Local<v8::Function> onMessage,
                         v8::Local<v8::Function> onSent)
    : m_asyncResource("isotp:EventCallbacks"),
      poll_(NULL),
      socket_(socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)),
      events_(0),
      m_errorCallback(onError),
      m_messageCallback(onMessage),
      m_sentCallback(onSent)
{
    assert(socket_);
    if (socket_ < 0)
    {
        perror("socket error");
    }

    // set socket options
    setsockopt(socket_, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &m_opts, sizeof(m_opts));
    setsockopt(socket_, SOL_CAN_ISOTP, CAN_ISOTP_RECV_FC, &m_fcopts, sizeof(m_fcopts));

    // set nonblocking mode
    int flags = fcntl(socket_, F_GETFL, 0);
    fcntl(socket_, F_SETFL, flags | O_NONBLOCK);

    m_recvBuffer = new isotp_frame;
    m_sendBuffer = new isotp_frame;

    poll_ = new uv_poll_t;
    int initRet = uv_poll_init_socket(uv_default_loop(), poll_, socket_);
    printf("initRet = %d\n", initRet);
    poll_->data = this;
}

void IsoTpSocket::Initialize(Local<Object> exports)
{
    Nan::HandleScope scope;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

    t->SetClassName(Nan::New("IsoTpSocket").ToLocalChecked());
    t->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(t, "bind", IsoTpSocket::Bind);
    Nan::SetPrototypeMethod(t, "start", IsoTpSocket::Start);
    Nan::SetPrototypeMethod(t, "close", IsoTpSocket::Close);
    Nan::SetPrototypeMethod(t, "send", IsoTpSocket::Send);

    constructor.Reset(t->GetFunction());
    exports->Set(Nan::New("IsoTpSocket").ToLocalChecked(), t->GetFunction());
}

void IsoTpSocket::New(const Nan::FunctionCallbackInfo<Value> &args)
{
    Nan::HandleScope scope;
    Isolate *isolate = args.GetIsolate();
    if (args.IsConstructCall())
    {
        if (args.Length() < 3)
        {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Wrong number of arguments")));
            return;
        }

        if (!args[0]->IsFunction()) // onError
        {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Invalid onError callback specified")));
            return;
        }

        if (!args[1]->IsFunction()) // onMessage
        {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Invalid onMessage callback specified")));
            return;
        }

        if (!args[2]->IsFunction()) // onSent
        {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Invalid onSent callback specified")));
            return;
        }

        // Invoked as constructor: `new IsoTpSocket(...)`
        IsoTpSocket *s = new IsoTpSocket(args[0].As<v8::Function>(),
                                         args[1].As<v8::Function>(),
                                         args[2].As<v8::Function>());
        s->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
    }
    else
    {
        // Invoked as plain function `IsoTpSocket(...)`, turn into construct call.
        Local<Function> cons = Nan::New<Function>(constructor);
        args.GetReturnValue().Set(Nan::NewInstance(cons).ToLocalChecked());
    }
}

void IsoTpSocket::Callback(uv_poll_t *w, int status, int revents)
{
    Nan::HandleScope scope;

    IsoTpSocket *self = static_cast<IsoTpSocket *>(w->data);
    assert(w == self->poll_);
    assert(self->socket_);

    // printf("uv_callback status=%d revents=%d\n", status, revents);

    if (status == 0)
    {
        if (revents & UV_WRITABLE)
        {
            // printf("UV_WRITABLE, nice! EWOULDBLOCK=%d\n", EWOULDBLOCK);
            ssize_t sentBytes = send(self->socket_, self->m_sendBuffer->data, self->m_sendBuffer->length, MSG_DONTWAIT);
            int err = (sentBytes < 0) ? errno : 0;

            if (err)
            {
                if (err == EWOULDBLOCK)
                {
                    // printf("EWOULDBLOCK\n");
                    return;
                }
                // perror("send error");
            }
            else
            {
                self->events_ &= ~UV_WRITABLE;
                self->StartInternal();
            }

            printf("send err=%d\n", err);
            self->CallbackOnSent(0);
        }
        else if (revents & UV_READABLE)
        {
            ssize_t recvBytes = recv(self->socket_, self->m_recvBuffer->data, MAX_PDU_LENGTH + 1, MSG_DONTWAIT);
            int err = (recvBytes < 0) ? errno : 0;

            if (err)
            {
                perror("recv error");
            }
            else
            {
                printf("recv %d\n", err);
                self->m_recvBuffer->length = recvBytes;
                self->CallbackOnMessage(self->m_recvBuffer);
            }
        }
    }
    else
    {
        printf("poll returned %d\n", status);
        self->CallbackOnError(status);
    }

    // Local<String> callback_symbol = Nan::New("callback").ToLocalChecked();
    // Local<Value> callback_v = Nan::Get(self->handle(), callback_symbol).ToLocalChecked();
    // if (!callback_v->IsFunction())
    // {
    //     self->CloseInternal();
    //     return;
    // }

    // Local<Function> callback = Local<Function>::Cast(callback_v);

    // const unsigned argc = 2;
    // Local<Value> argv[argc] = {
    //     revents & UV_READABLE ? Nan::True() : Nan::False(),
    //     revents & UV_WRITABLE ? Nan::True() : Nan::False(),
    // };

    // Nan::MakeCallback(self->handle(), callback, argc, argv);
}

void IsoTpSocket::Bind(const Nan::FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = args.GetIsolate();
    if (args.Length() < 3)
    {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong number of arguments")));
        return;
    }
    IsoTpSocket *self = Nan::ObjectWrap::Unwrap<IsoTpSocket>(args.Holder());
    assert(self);
    // assert(!self->m_closed);

    if (!self->socket_)
        return;

    Nan::Utf8String iface(args[0]->ToString());

    assert(args[1]->IsNumber());
    auto tx_id = (unsigned long int)args[1]->NumberValue();
    assert(tx_id <= 0x7FFU);

    assert(args[2]->IsNumber());
    auto rx_id = (unsigned long int)args[2]->NumberValue();
    assert(rx_id <= 0x7FFU);

    auto ifr = ifreq();
    strcpy(ifr.ifr_name, *iface);
    auto err = ioctl(self->socket_, SIOCGIFINDEX, &ifr);

    if (err == 0)
    {
        auto canAddr = sockaddr_can();
        canAddr.can_family = AF_CAN;
        canAddr.can_ifindex = ifr.ifr_ifindex;
        canAddr.can_addr.tp.tx_id = tx_id;
        canAddr.can_addr.tp.rx_id = rx_id;

        err = bind(self->socket_, reinterpret_cast<struct sockaddr *>(&canAddr),
                   sizeof(canAddr));
    }

    self->Ref();

    self->events_ |= UV_READABLE;

    args.GetReturnValue().Set(err);
}

void IsoTpSocket::Start(const Nan::FunctionCallbackInfo<Value> &info)
{
    IsoTpSocket *self = Nan::ObjectWrap::Unwrap<IsoTpSocket>(info.Holder());
    self->StartInternal();
}

void IsoTpSocket::StartInternal()
{
    if (poll_ == NULL)
    {
        poll_ = new uv_poll_t;
        memset(poll_, 0, sizeof(uv_poll_t));
        poll_->data = this;
        uv_poll_init_socket(uv_default_loop(), poll_, socket_);

        Ref();
    }

    // if (!uv_is_active((uv_handle_t *)poll_))
    // {
    uv_poll_start(poll_, events_, &IsoTpSocket::Callback);
    // }
}

void IsoTpSocket::Close(const Nan::FunctionCallbackInfo<Value> &info)
{
    IsoTpSocket *self = Nan::ObjectWrap::Unwrap<IsoTpSocket>(info.Holder());
    self->CloseInternal();
}

void IsoTpSocket::CloseInternal()
{
    events_ = 0;
    if (socket_ != 0 && poll_ != NULL && uv_is_active((uv_handle_t *)poll_))
    {
        uv_poll_stop(poll_);

        uv_close(reinterpret_cast<uv_handle_t *>(poll_),
                 [](uv_handle_t *handle) {
                     auto *self = reinterpret_cast<IsoTpSocket *>(handle->data);
                     assert(!self->persistent().IsEmpty());
                     self->CloseSocketInternal();
                     self->Unref();
                 });
    }
}

void IsoTpSocket::CloseSocketInternal()
{
    if (!socket_)
        return;
    close(socket_);
    socket_ = 0;
}

void IsoTpSocket::Send(const Nan::FunctionCallbackInfo<Value> &info)
{
    assert(1 == info.Length());
    assert(node::Buffer::HasInstance(info[0]));

    IsoTpSocket *self = Nan::ObjectWrap::Unwrap<IsoTpSocket>(info.Holder());
    assert(self);

    self->SendInternal(node::Buffer::Data(info[0]), node::Buffer::Length(info[0]));
}

void IsoTpSocket::SendInternal(char *data, size_t length)
{
    if (!socket_)
        return;
    m_sendBuffer->length = length;
    std::copy_n(data, length, std::begin(m_sendBuffer->data));

    printf("send internal len=%d\n", length);

    events_ |= UV_WRITABLE;
    StartInternal();
}

void IsoTpSocket::CallbackOnError(int err)
{
    const unsigned argc = 1;
    Local<Value> argv[argc] = {Nan::New(err)};
    m_errorCallback.Call(handle(), argc, argv, &m_asyncResource);
}

void IsoTpSocket::CallbackOnMessage(isotp_frame *frame)
{
    const unsigned argc = 1;
    Local<Value> argv[argc] = {Nan::CopyBuffer(reinterpret_cast<char *>(frame->data), frame->length).ToLocalChecked()};
    m_messageCallback.Call(handle(), argc, argv, &m_asyncResource);
}

void IsoTpSocket::CallbackOnSent(int err)
{
    const unsigned argc = 1;
    Local<Value> argv[argc] = {Nan::New(err)};
    m_sentCallback.Call(handle(), argc, argv, &m_asyncResource);
}

// v8::Local<v8::Value> IsoTpSocket::MakeCallback(const char *function, int argc, v8::Local<v8::Value> *argv)
// {
//     Local<String> callback_symbol = Nan::New(function).ToLocalChecked();
//     Local<Value> callback_v = Nan::Get(self->handle(), callback_symbol).ToLocalChecked();
//     if (!callback_v->IsFunction())
//     {
//         self->CloseInternal();
//         return;
//     }

//     Local<Function> callback = Local<Function>::Cast(callback_v);

//     return Nan::MakeCallback(handle(), callback, argc, argv);
// }

void Initialize(Local<Object> exports)
{
    IsoTpSocket::Initialize(exports);
}

NODE_MODULE(isotp, Initialize);