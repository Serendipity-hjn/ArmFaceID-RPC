#include <functional>

#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/support/status.h>

#include "proto.grpc.pb.h"

namespace arm_face_id {

template <typename Req, typename Resp>
void RequestRpc(FaceRpc::AsyncService* service, grpc::ServerContext* ctx,
                Req& req, grpc::ServerAsyncResponseWriter<Resp>& resp,
                ::grpc::CompletionQueue* new_call_cq,
                ::grpc::ServerCompletionQueue* notification_cq, void* tag) {}

template <>
inline void RequestRpc(
    FaceRpc::AsyncService* service, grpc::ServerContext* ctx,
    RecognitionRequest& req,
    grpc::ServerAsyncResponseWriter<RecognitionResponse>& resp,
    ::grpc::CompletionQueue* new_call_cq,
    ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
  service->RequestRpcRecognizeFace(ctx, &req, &resp, new_call_cq,
                                   notification_cq, tag);
}

class RPCHandlerBase {
 public:
  virtual void Proceed() = 0;
};

template <typename Req, typename Resp>
using Call = std::function<grpc::Status(Req&, Resp&)>;

template <typename Req, typename Resp>
class RPCHandler : public RPCHandlerBase {
 private:
  friend class RpcServer;

 public:
  RPCHandler(FaceRpc::AsyncService* service, grpc::ServerCompletionQueue* cq,
             Call<Req, Resp> handler)
      : service_(service),
        cq_(cq),
        responder_(&ctx_),
        status_(CREATE),
        on_process_func_(handler) {
    Proceed();
  }

  void Proceed() override {
    if (status_ == CREATE) {
      status_ = PROCESS;
      RequestRpc(service_, &ctx_, request_, responder_, cq_, cq_, this);
    } else if (status_ == PROCESS) {
      new RPCHandler(service_, cq_, on_process_func_);
      // absl::CivilSecond civil_second =
      //     absl::ToCivilSecond(absl::Now(), absl::LocalTimeZone());
      // std::string formatted_str = absl::FormatCivilTime(civil_second);
      // // 处理业务逻辑
      // spdlog::info("RPC 服务端：处理一条请求。{}", formatted_str);
      auto grpc_status = on_process_func_(request_, reply_);
      status_ = FINISH;
      responder_.Finish(reply_, grpc_status, this);
    } else {
      delete this;
    }
  }

 private:
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  arm_face_id::FaceRpc::AsyncService* service_;
  grpc::ServerCompletionQueue* cq_;
  grpc::ServerContext ctx_;

  Call<Req, Resp> on_process_func_;
  Req request_;
  Resp reply_;
  grpc::ServerAsyncResponseWriter<Resp> responder_;
};

}  // namespace arm_face_id