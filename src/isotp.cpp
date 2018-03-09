#include "isotp.hpp"

using namespace v8;

Nan::Persistent<Function> constructor;

IsoTpSocket::IsoTpSocket()
    : poll_(NULL),
      socket_(socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)),
      events_(0)
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
    Nan::SetPrototypeMethod(t, "stop", IsoTpSocket::Stop);
    Nan::SetPrototypeMethod(t, "send", IsoTpSocket::Send);

    constructor.Reset(t->GetFunction());
    exports->Set(Nan::New("IsoTpSocket").ToLocalChecked(), t->GetFunction());
}

void IsoTpSocket::New(const Nan::FunctionCallbackInfo<Value> &info)
{
    Nan::HandleScope scope;
    if (info.IsConstructCall())
    {
        // Invoked as constructor: `new IsoTpSocket(...)`
        IsoTpSocket *s = new IsoTpSocket();
        s->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }
    else
    {
        // Invoked as plain function `IsoTpSocket(...)`, turn into construct call.
        Local<Function> cons = Nan::New<Function>(constructor);
        info.GetReturnValue().Set(Nan::NewInstance(cons).ToLocalChecked());
    }
}

void IsoTpSocket::Callback(uv_poll_t *w, int status, int revents)
{
    Nan::HandleScope scope;

    IsoTpSocket *self = static_cast<IsoTpSocket *>(w->data);
    assert(w == self->poll_);

    if (status == 0)
    {
        if (revents & UV_WRITABLE)
        {
            ssize_t err = send(self->socket_, self->m_sendBuffer->data, self->m_sendBuffer->length, MSG_DONTWAIT);
            if (err < 0)
            {
                if (err == EWOULDBLOCK)
                {
                    return;
                }
                perror("recv error");
            }
            else
            {
                printf("sent %d\n", err);
                self->events_ &= ~UV_WRITABLE;
                self->StartInternal();
            }
        }
        else if (revents & UV_READABLE)
        {
            ssize_t err = recv(self->socket_, self->m_recvBuffer->data, MAX_PDU_LENGTH + 1, MSG_DONTWAIT);
            if (err < 0)
            {
                perror("recv error");
            }
            else
            {
                self->m_recvBuffer->length = err;
                printf("recv %d\n", err);

                Local<String> callback_symbol = Nan::New("callback").ToLocalChecked();
                Local<Value> callback_v = Nan::Get(self->handle(), callback_symbol).ToLocalChecked();
                if (!callback_v->IsFunction())
                {
                    self->StopInternal();
                    return;
                }

                Local<Function> callback = Local<Function>::Cast(callback_v);

                const unsigned argc = 1;
                Local<Value> argv[argc];
                argv[0] = Nan::CopyBuffer(reinterpret_cast<char *>(self->m_recvBuffer->data), self->m_recvBuffer->length).ToLocalChecked();

                Nan::MakeCallback(self->handle(), callback, argc, argv);
            }
        }
    }
    else
    {
        printf("poll returned %d\n", status);
    }

    // Local<String> callback_symbol = Nan::New("callback").ToLocalChecked();
    // Local<Value> callback_v = Nan::Get(self->handle(), callback_symbol).ToLocalChecked();
    // if (!callback_v->IsFunction())
    // {
    //     self->StopInternal();
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

void IsoTpSocket::Bind(const Nan::FunctionCallbackInfo<Value> &info)
{
    assert(3 == info.Length());
    IsoTpSocket *self = Nan::ObjectWrap::Unwrap<IsoTpSocket>(info.Holder());

    assert(self);
    // assert(!self->m_closed);

    Nan::Utf8String iface(info[0]->ToString());

    assert(info[1]->IsNumber());
    auto tx_id = (unsigned long int)info[1]->NumberValue();
    assert(tx_id <= 0x7FFU);

    assert(info[2]->IsNumber());
    auto rx_id = (unsigned long int)info[2]->NumberValue();
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

    info.GetReturnValue().Set(err);
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

void IsoTpSocket::Stop(const Nan::FunctionCallbackInfo<Value> &info)
{
    IsoTpSocket *self = Nan::ObjectWrap::Unwrap<IsoTpSocket>(info.Holder());
    self->StopInternal();
}

void IsoTpSocket::StopInternal()
{
    if (poll_ != NULL && uv_is_active((uv_handle_t *)poll_))
    {
        uv_poll_stop(poll_);
        Unref();
    }
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
    m_sendBuffer->length = length;
    std::copy_n(data, length, std::begin(m_sendBuffer->data));

    events_ |= UV_WRITABLE;
    StartInternal();
}

void Initialize(Local<Object> exports)
{
    IsoTpSocket::Initialize(exports);
}

NODE_MODULE(isotp, Initialize);