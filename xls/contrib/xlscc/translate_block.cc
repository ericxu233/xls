// Copyright 2022 The XLS Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "clang/include/clang/AST/Decl.h"
#include "clang/include/clang/AST/DeclCXX.h"
#include "clang/include/clang/AST/GlobalDecl.h"
#include "clang/include/clang/AST/Type.h"
#include "clang/include/clang/Basic/LLVM.h"
#include "xls/common/logging/logging.h"
#include "xls/common/status/status_macros.h"
#include "xls/contrib/xlscc/hls_block.pb.h"
#include "xls/contrib/xlscc/translator.h"
#include "xls/contrib/xlscc/xlscc_logging.h"
#include "xls/ir/bits.h"
#include "xls/ir/channel.h"
#include "xls/ir/channel_ops.h"
#include "xls/ir/function_builder.h"
#include "xls/ir/package.h"
#include "xls/ir/source_location.h"
#include "xls/ir/value.h"
#include "xls/ir/value_helpers.h"

using std::shared_ptr;
using std::string;
using std::vector;

namespace xlscc {

absl::Status Translator::GenerateExternalChannels(
    std::list<ExternalChannelInfo>& top_decls,
    absl::flat_hash_map<const clang::NamedDecl*, ChannelBundle>*
        top_channel_injections,
    const xls::SourceInfo& loc) {
  XLS_CHECK_NE(top_channel_injections, nullptr);
  for (ExternalChannelInfo& top_decl : top_decls) {
    const clang::NamedDecl* decl = top_decl.decl;
    std::shared_ptr<CChannelType> channel_type = top_decl.channel_type;
    XLS_CHECK_NE(channel_type, nullptr);

    XLS_ASSIGN_OR_RETURN(xls::Type * data_type,
                         TranslateTypeToXLS(channel_type->GetItemType(), loc));

    ChannelBundle new_channel;

    if (top_decl.interface_type == InterfaceType::kFIFO) {
      const auto xls_channel_op = top_decl.is_input
                                      ? xls::ChannelOps::kReceiveOnly
                                      : xls::ChannelOps::kSendOnly;

      XLS_ASSIGN_OR_RETURN(
          new_channel.regular,
          package_->CreateStreamingChannel(
              decl->getNameAsString(), xls_channel_op, data_type,
              /*initial_values=*/{}, /*fifo_config=*/std::nullopt,
              xls::FlowControl::kReadyValid,
              // TODO(google/xls#1023): Make channel strictness
              // frontend-configurable.
              /*strictness=*/xls::ChannelStrictness::kArbitraryStaticOrder));
      unused_external_channels_.push_back(new_channel.regular);
    } else if (top_decl.interface_type == InterfaceType::kDirect) {
      XLS_CHECK(top_decl.is_input);
      XLS_ASSIGN_OR_RETURN(new_channel.regular,
                           package_->CreateSingleValueChannel(
                               decl->getNameAsString(),
                               xls::ChannelOps::kReceiveOnly, data_type));
      unused_external_channels_.push_back(new_channel.regular);
    } else if (top_decl.interface_type == InterfaceType::kMemory) {
      const std::string& memory_name = top_decl.decl->getNameAsString();

      XLS_ASSIGN_OR_RETURN(
          xls::Type * read_request_type,
          top_decl.channel_type->GetReadRequestType(package_, data_type));
      XLS_ASSIGN_OR_RETURN(
          new_channel.read_request,
          package_->CreateStreamingChannel(
              memory_name + "__read_request", xls::ChannelOps::kSendOnly,
              read_request_type,
              /*initial_values=*/{}, /*fifo_config=*/std::nullopt,
              xls::FlowControl::kReadyValid,
              // TODO(google/xls#1023): Make channel strictness
              // frontend-configurable.
              /*strictness=*/xls::ChannelStrictness::kArbitraryStaticOrder));
      unused_external_channels_.push_back(new_channel.read_request);

      XLS_ASSIGN_OR_RETURN(
          xls::Type * read_response_type,
          top_decl.channel_type->GetReadResponseType(package_, data_type));
      XLS_ASSIGN_OR_RETURN(
          new_channel.read_response,
          package_->CreateStreamingChannel(
              memory_name + "__read_response", xls::ChannelOps::kReceiveOnly,
              read_response_type,
              /*initial_values=*/{}, /*fifo_config=*/std::nullopt,
              xls::FlowControl::kReadyValid,
              // TODO(google/xls#1023): Make channel strictness
              // frontend-configurable.
              /*strictness=*/xls::ChannelStrictness::kArbitraryStaticOrder));
      unused_external_channels_.push_back(new_channel.read_response);

      XLS_ASSIGN_OR_RETURN(
          xls::Type * write_request_type,
          top_decl.channel_type->GetWriteRequestType(package_, data_type));
      XLS_ASSIGN_OR_RETURN(
          new_channel.write_request,
          package_->CreateStreamingChannel(
              memory_name + "__write_request", xls::ChannelOps::kSendOnly,
              write_request_type,
              /*initial_values=*/{}, /*fifo_config=*/std::nullopt,
              xls::FlowControl::kReadyValid,
              // TODO(google/xls#1023): Make channel strictness
              // frontend-configurable.
              /*strictness=*/xls::ChannelStrictness::kArbitraryStaticOrder));
      unused_external_channels_.push_back(new_channel.write_request);

      XLS_ASSIGN_OR_RETURN(
          xls::Type * write_response_type,
          top_decl.channel_type->GetWriteResponseType(package_, data_type));
      XLS_ASSIGN_OR_RETURN(
          new_channel.write_response,
          package_->CreateStreamingChannel(
              memory_name + "__write_response", xls::ChannelOps::kReceiveOnly,
              write_response_type,
              /*initial_values=*/{}, /*fifo_config=*/std::nullopt,
              xls::FlowControl::kReadyValid,
              // TODO(google/xls#1023): Make channel strictness
              // frontend-configurable.
              /*strictness=*/xls::ChannelStrictness::kArbitraryStaticOrder));
      unused_external_channels_.push_back(new_channel.write_response);
    } else {
      return absl::InvalidArgumentError(
          ErrorMessage(GetLoc(*decl), "Unknown interface type for channel %s",
                       decl->getNameAsString()));
    }
    top_decl.external_channels = new_channel;
    if (top_decl.interface_type != InterfaceType::kDirect) {
      (*top_channel_injections)[decl] = new_channel;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<xls::Proc*> Translator::GenerateIR_Block(
    xls::Package* package, const HLSBlock& block, int top_level_init_interval) {
  package_ = package;

  absl::flat_hash_map<std::string, HLSChannel> channels_by_name;
  for (const HLSChannel& channel : block.channels()) {
    if (!channel.has_name() || !channel.has_type()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Channel is incomplete in proto"));
    }

    if (channels_by_name.contains(channel.name())) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate channel name %s", channel.name()));
    }

    channels_by_name[channel.name()] = channel;
  }

  const clang::FunctionDecl* top_function = nullptr;

  XLS_CHECK_NE(parser_.get(), nullptr);
  XLS_ASSIGN_OR_RETURN(top_function, parser_->GetTopFunction());

  const clang::FunctionDecl* definition = nullptr;
  top_function->getBody(definition);
  const xls::SourceInfo body_loc = GetLoc(*definition);

  std::list<ExternalChannelInfo> top_decls;

  for (int pidx = 0; pidx < definition->getNumParams(); ++pidx) {
    const clang::ParmVarDecl* param = definition->getParamDecl(pidx);

    if (!channels_by_name.contains(param->getNameAsString())) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Parameter %s doesn't name a channel",
                          param->getNameAsString().c_str()));
    }

    const HLSChannel& channel_spec =
        channels_by_name.at(param->getNameAsString());

    XLS_CHECK(channel_spec.type() == ChannelType::DIRECT_IN ||
              channel_spec.type() == ChannelType::FIFO ||
              channel_spec.type() == ChannelType::MEMORY);

    ExternalChannelInfo channel_info = {.decl = param};

    if (channel_spec.type() == ChannelType::DIRECT_IN) {
      channel_info.interface_type = InterfaceType::kDirect;
      XLS_ASSIGN_OR_RETURN(StrippedType stripped,
                           StripTypeQualifiers(param->getType()));
      XLS_ASSIGN_OR_RETURN(
          auto ctype, TranslateTypeFromClang(stripped.base, GetLoc(*param)));
      channel_info.channel_type =
          std::make_shared<CChannelType>(ctype, /*memory_size=*/-1);
      channel_info.extra_return =
          stripped.is_ref && !stripped.base.isConstQualified();
      channel_info.is_input = channel_spec.is_input();
    } else if (channel_spec.type() == ChannelType::FIFO) {
      channel_info.interface_type = InterfaceType::kFIFO;
      XLS_ASSIGN_OR_RETURN(
          channel_info.channel_type,
          GetChannelType(param->getType(), param->getASTContext(),
                         GetLoc(*param)));

      channel_info.channel_type = std::make_shared<CChannelType>(
          channel_info.channel_type->GetItemType(),
          /*memory_size=*/-1);
      channel_info.is_input = channel_spec.is_input();
    } else if (channel_spec.type() == ChannelType::MEMORY) {
      channel_info.interface_type = InterfaceType::kMemory;
      XLS_ASSIGN_OR_RETURN(
          channel_info.channel_type,
          GetChannelType(param->getType(), param->getASTContext(),
                         GetLoc(*param)));

      XLS_CHECK_EQ(channel_spec.depth(),
                   channel_info.channel_type->GetMemorySize());
    } else {
      return absl::InvalidArgumentError(ErrorMessage(
          GetLoc(*param),
          "Don't know how to interpret channel type %i for param %s",
          static_cast<int>(channel_spec.type()), param->getNameAsString()));
    }

    top_decls.push_back(channel_info);
  }

  return GenerateIR_Block(package, block, /*this_type=*/nullptr,
                          /*this_decl=*/nullptr, top_decls, body_loc,
                          top_level_init_interval,
                          /*force_static=*/true,
                          /*member_references_become_channels=*/false);
}

absl::StatusOr<xls::Proc*> Translator::GenerateIR_Block(
    xls::Package* package, const HLSBlock& block,
    const std::shared_ptr<CType>& this_type,
    const clang::CXXRecordDecl* this_decl,
    std::list<ExternalChannelInfo>& top_decls, const xls::SourceInfo& body_loc,
    int top_level_init_interval, bool force_static,
    bool member_references_become_channels) {
  XLS_CHECK_NE(package_, nullptr);

  // Create external channels
  XLS_RETURN_IF_ERROR(
      CheckInitIntervalValidity(top_level_init_interval, body_loc));

  XLS_RETURN_IF_ERROR(GenerateIRBlockCheck(block, top_decls, body_loc));
  absl::flat_hash_map<const clang::NamedDecl*, ChannelBundle>
      top_channel_injections;
  XLS_RETURN_IF_ERROR(
      GenerateExternalChannels(top_decls, &top_channel_injections, body_loc));

  // Generate function without FIFO channel parameters
  // Force top function in block to be static.
  PreparedBlock prepared;

  XLS_ASSIGN_OR_RETURN(
      prepared.xls_func,
      GenerateIR_Top_Function(package,
                              /*top_channel_injections=*/top_channel_injections,
                              force_static, member_references_become_channels,
                              top_level_init_interval));

  xls::ProcBuilder pb(block.name() + "_proc", /*token_name=*/"tkn", package);

  prepared.token = pb.GetTokenParam();

  XLS_RETURN_IF_ERROR(GenerateIRBlockPrepare(prepared, pb,
                                             /*next_return_index=*/0,
                                             /*next_state_index=*/0, this_type,
                                             this_decl, top_decls, body_loc));

  XLS_ASSIGN_OR_RETURN(xls::BValue last_ret_val,
                       GenerateIOInvokes(prepared, pb, body_loc));

  // Generate default ops for unused external channels
  XLS_RETURN_IF_ERROR(GenerateDefaultIOOps(prepared, pb, body_loc));

  // Create next state value
  std::vector<xls::BValue> static_next_values;
  XLS_CHECK_GE(prepared.xls_func->return_value_count,
               prepared.xls_func->static_values.size());

  XLS_CHECK(context().fb == &pb);

  if (this_decl != nullptr) {
    xls::BValue next_val = GetFlexTupleField(
        last_ret_val, prepared.return_index_for_static.at(this_decl),
        prepared.xls_func->return_value_count, body_loc);
    static_next_values.push_back(next_val);
  }

  for (const clang::NamedDecl* namedecl :
       prepared.xls_func->GetDeterministicallyOrderedStaticValues()) {
    xls::BValue next_val = GetFlexTupleField(
        last_ret_val, prepared.return_index_for_static[namedecl],
        prepared.xls_func->return_value_count, body_loc);

    XLS_ASSIGN_OR_RETURN(bool is_on_reset, DeclIsOnReset(namedecl));
    if (is_on_reset) {
      next_val = pb.Literal(xls::Value(xls::UBits(0, 1)), body_loc);
    }
    static_next_values.push_back(next_val);
  }
  XLS_CHECK_EQ(static_next_values.size(), prepared.state_init_count);

  return pb.Build(prepared.token, static_next_values);
}

absl::StatusOr<xls::Proc*> Translator::GenerateIR_BlockFromClass(
    xls::Package* package, HLSBlock* block_spec_out,
    int top_level_init_interval) {
  package_ = package;
  block_spec_out->Clear();

  // Create external channels
  const clang::FunctionDecl* top_function = nullptr;

  XLS_CHECK_NE(parser_.get(), nullptr);
  XLS_ASSIGN_OR_RETURN(top_function, parser_->GetTopFunction());

  if (!clang::isa<clang::CXXMethodDecl>(top_function)) {
    return absl::InvalidArgumentError(
        ErrorMessage(GetLoc(*top_function), "Top function %s isn't a method",
                     top_function->getQualifiedNameAsString().c_str()));
  }

  auto method = clang::dyn_cast<clang::CXXMethodDecl>(top_function);
  const clang::QualType& this_type = method->getThisType()->getPointeeType();

  XLS_CHECK(this_type->isRecordType());

  if (this_type.isConstQualified()) {
    return absl::UnimplementedError(
        ErrorMessage(GetLoc(*method), "Const top method unsupported"));
  }

  if (!top_function->getReturnType()->isVoidType()) {
    return absl::UnimplementedError(ErrorMessage(
        GetLoc(*method), "Non-void top method return unsupported"));
  }
  if (!top_function->parameters().empty()) {
    return absl::UnimplementedError(
        ErrorMessage(GetLoc(*method), "Top method parameters unsupported"));
  }

  const clang::CXXRecordDecl* record_decl = this_type->getAsCXXRecordDecl();

  context().ast_context = &record_decl->getASTContext();

  XLS_RETURN_IF_ERROR(ScanStruct(record_decl));

  XLS_ASSIGN_OR_RETURN(
      shared_ptr<CType> this_ctype,
      TranslateTypeFromClang(this_type, GetLoc(*top_function)));

  XLS_ASSIGN_OR_RETURN(std::shared_ptr<CType> struct_ctype,
                       ResolveTypeInstance(this_ctype));

  auto struct_type = std::dynamic_pointer_cast<CStructType>(struct_ctype);
  XLS_CHECK_NE(struct_type, nullptr);

  block_spec_out->set_name(record_decl->getNameAsString());

  std::list<ExternalChannelInfo> top_decls;
  for (const clang::FieldDecl* field_decl : record_decl->fields()) {
    std::shared_ptr<CField> field = struct_type->get_field(field_decl);
    std::shared_ptr<CType> field_type = field->type();

    // Only uninitialized are external channels
    if (field_decl->hasInClassInitializer()) {
      continue;
    }

    XLS_ASSIGN_OR_RETURN(std::shared_ptr<CType> resolved_field_type,
                         ResolveTypeInstanceDeeply(field->type()));

    if (auto channel_type =
            std::dynamic_pointer_cast<CChannelType>(field->type())) {
      if (channel_type->GetOpType() != OpType::kNull) {
        if (channel_type->GetOpType() == OpType::kSendRecv) {
          return absl::UnimplementedError(
              ErrorMessage(GetLoc(*field->name()),
                           "Internal (InOut) channels in top class"));
        }

        xlscc::HLSChannel* channel_spec = block_spec_out->add_channels();
        channel_spec->set_name(field->name()->getNameAsString());
        channel_spec->set_width_in_bits(resolved_field_type->GetBitWidth());
        channel_spec->set_type(xlscc::FIFO);
        channel_spec->set_is_input(channel_type->GetOpType() == OpType::kRecv);

        ExternalChannelInfo channel_info = {
            .decl = field->name(),
            .channel_type = channel_type,
            .interface_type = InterfaceType::kFIFO,
            .is_input = channel_type->GetOpType() == OpType::kRecv};
        top_decls.push_back(channel_info);
      } else if (channel_type->GetMemorySize() > 0) {
        xlscc::HLSChannel* channel_spec = block_spec_out->add_channels();
        channel_spec->set_name(field->name()->getNameAsString());
        channel_spec->set_width_in_bits(resolved_field_type->GetBitWidth());
        channel_spec->set_type(xlscc::MEMORY);
        channel_spec->set_depth(channel_type->GetMemorySize());

        ExternalChannelInfo channel_info = {
            .decl = field->name(),
            .channel_type = channel_type,
            .interface_type = InterfaceType::kMemory};
        top_decls.push_back(channel_info);
      } else {
        return absl::InvalidArgumentError(
            ErrorMessage(GetLoc(*field->name()),
                         "Direction or depth unspecified for external channel "
                         "or memory '%s'",
                         field->name()->getNameAsString().c_str()));
      }
    } else if (auto channel_type =
                   std::dynamic_pointer_cast<CReferenceType>(field->type())) {
      xlscc::HLSChannel* channel_spec = block_spec_out->add_channels();
      channel_spec->set_name(field->name()->getNameAsString());
      channel_spec->set_width_in_bits(resolved_field_type->GetBitWidth());
      channel_spec->set_type(xlscc::DIRECT_IN);
      channel_spec->set_is_input(true);

      ExternalChannelInfo channel_info = {
          .decl = field->name(),
          .channel_type =
              std::make_shared<CChannelType>(channel_type->GetPointeeType(),
                                             /*memory_size=*/-1),
          .interface_type = InterfaceType::kDirect,
          .is_input = true};
      top_decls.push_back(channel_info);
    }
  }

  return GenerateIR_Block(package, *block_spec_out, this_ctype,
                          /*this_decl=*/record_decl, top_decls,
                          GetLoc(*record_decl), top_level_init_interval,
                          /*force_static=*/false,
                          /*member_references_become_channels=*/true);
}

absl::Status Translator::GenerateDefaultIOOp(
    xls::Channel* channel, std::vector<xls::BValue>& final_tokens,
    xls::ProcBuilder& pb, const xls::SourceInfo& loc) {
  xls::BValue token;

  xls::BValue pred_0 = pb.Literal(xls::UBits(0, 1), loc);

  xls::Value data_0_val = xls::ZeroOfType(channel->type());
  xls::BValue data_0 = pb.Literal(data_0_val, loc);

  if (channel->CanReceive()) {
    xls::BValue tup = pb.ReceiveIf(channel, pb.GetTokenParam(), pred_0, loc);
    token = pb.TupleIndex(tup, 0);
  } else if (channel->CanSend()) {
    token = pb.SendIf(channel, pb.GetTokenParam(), pred_0, data_0, loc);
  } else {
    return absl::UnimplementedError(ErrorMessage(
        loc, "Don't know how to create default IO op for channel %s",
        channel->name()));
  }
  final_tokens.push_back(token);
  return absl::OkStatus();
}

absl::Status Translator::GenerateDefaultIOOps(PreparedBlock& prepared,
                                              xls::ProcBuilder& pb,
                                              const xls::SourceInfo& body_loc) {
  if (unused_external_channels_.empty()) {
    return absl::OkStatus();
  }

  std::vector<xls::BValue> final_tokens = {prepared.token};

  for (xls::Channel* channel : unused_external_channels_) {
    XLS_RETURN_IF_ERROR(
        GenerateDefaultIOOp(channel, final_tokens, pb, body_loc));
  }

  prepared.token = pb.AfterAll(final_tokens, body_loc);

  return absl::OkStatus();
}

absl::StatusOr<xls::BValue> Translator::GenerateIOInvokes(
    PreparedBlock& prepared, xls::ProcBuilder& pb,
    const xls::SourceInfo& body_loc) {
  XLS_CHECK(&pb == context().fb);

  XLS_CHECK_GE(prepared.xls_func->return_value_count,
               prepared.xls_func->io_ops.size());

  // The function is first invoked with defaults for any
  //  read() IO Ops.
  // If there are any read() IO Ops, then it will be invoked again
  //  for each read Op below.
  // Statics don't need to generate additional invokes, since they need not
  //  exchange any data with the outside world between iterations.
  xls::BValue last_ret_val =
      pb.Invoke(prepared.args, prepared.xls_func->xls_func, body_loc);

  // Returns token
  auto generate_op_invoke =
      [this, &prepared, &last_ret_val, &pb](
          const IOOp& op,
          xls::BValue before_token) -> absl::StatusOr<xls::BValue> {
    xls::SourceInfo op_loc = op.op_location;
    const int return_index = prepared.return_index_for_op.at(&op);

    xls::BValue new_token;
    const ChannelBundle* bundle_ptr = nullptr;

    if (op.op != OpType::kTrace) {
      bundle_ptr = &prepared.xls_channel_by_function_channel.at(op.channel);
    }

    if (op.op == OpType::kRecv) {
      xls::Channel* xls_channel = bundle_ptr->regular;

      unused_external_channels_.remove(xls_channel);

      XLS_CHECK_NE(xls_channel, nullptr);
      const int arg_index = prepared.arg_index_for_op.at(&op);
      XLS_CHECK(arg_index >= 0 && arg_index < prepared.args.size());

      xls::BValue condition = GetFlexTupleField(
          last_ret_val, return_index, prepared.xls_func->return_value_count,
          op_loc, absl::StrFormat("%s_pred", xls_channel->name()));
      XLS_CHECK_EQ(condition.GetType()->GetFlatBitCount(), 1);
      xls::BValue receive;
      if (op.is_blocking) {
        receive = pb.ReceiveIf(xls_channel, before_token, condition, op_loc);
      } else {
        receive = pb.ReceiveIfNonBlocking(xls_channel, before_token, condition,
                                          op_loc);
      }
      new_token = pb.TupleIndex(receive, 0);

      xls::BValue in_val;
      if (op.is_blocking) {
        in_val = pb.TupleIndex(receive, 1);
      } else {
        in_val =
            pb.Tuple({pb.TupleIndex(receive, 1), pb.TupleIndex(receive, 2)});
      }
      prepared.args[arg_index] = in_val;

      // The function is invoked again with the value received from the channel
      //  for each read() Op. The final invocation will produce all complete
      //  outputs.
      last_ret_val =
          pb.Invoke(prepared.args, prepared.xls_func->xls_func, op_loc);
    } else if (op.op == OpType::kSend) {
      xls::Channel* xls_channel = bundle_ptr->regular;

      unused_external_channels_.remove(xls_channel);

      XLS_CHECK_NE(xls_channel, nullptr);
      xls::BValue send_tup =
          GetFlexTupleField(last_ret_val, return_index,
                            prepared.xls_func->return_value_count, op_loc);
      xls::BValue val = pb.TupleIndex(send_tup, 0, op_loc);
      xls::BValue condition = pb.TupleIndex(
          send_tup, 1, op_loc, absl::StrFormat("%s_pred", xls_channel->name()));

      new_token = pb.SendIf(xls_channel, before_token, condition, val, op_loc);
    } else if (op.op == OpType::kRead) {
      XLS_CHECK_EQ(bundle_ptr->regular, nullptr);
      XLS_CHECK_NE(bundle_ptr->read_request, nullptr);
      XLS_CHECK_NE(bundle_ptr->read_response, nullptr);

      unused_external_channels_.remove(bundle_ptr->read_request);
      unused_external_channels_.remove(bundle_ptr->read_response);

      const int arg_index = prepared.arg_index_for_op.at(&op);
      XLS_CHECK(arg_index >= 0 && arg_index < prepared.args.size());

      xls::BValue read_tup =
          GetFlexTupleField(last_ret_val, return_index,
                            prepared.xls_func->return_value_count, op_loc);

      xls::BValue addr = pb.TupleIndex(read_tup, 0, op_loc);
      xls::BValue condition = pb.TupleIndex(read_tup, 1, op_loc);

      // TODO(google/xls#861): supported masked memory operations.
      xls::BValue mask = pb.Literal(xls::Value::Tuple({}), op_loc);
      xls::BValue send_tuple_with_mask = pb.Tuple({addr, mask}, op_loc);
      new_token = pb.SendIf(bundle_ptr->read_request, before_token, condition,
                            send_tuple_with_mask, op_loc);

      xls::BValue receive =
          pb.ReceiveIf(bundle_ptr->read_response, new_token, condition, op_loc);

      new_token = pb.TupleIndex(receive, 0);
      xls::BValue response_tup = pb.TupleIndex(receive, 1, op_loc);
      xls::BValue response = pb.TupleIndex(response_tup, 0, op_loc);

      prepared.args[arg_index] = response;

      // The function is invoked again with the value received from the channel
      //  for each read() Op. The final invocation will produce all complete
      //  outputs.
      last_ret_val =
          pb.Invoke(prepared.args, prepared.xls_func->xls_func, op_loc);
    } else if (op.op == OpType::kWrite) {
      XLS_CHECK_EQ(bundle_ptr->regular, nullptr);
      XLS_CHECK_NE(bundle_ptr->write_request, nullptr);
      XLS_CHECK_NE(bundle_ptr->write_response, nullptr);

      unused_external_channels_.remove(bundle_ptr->write_request);
      unused_external_channels_.remove(bundle_ptr->write_response);

      xls::BValue send_tup =
          GetFlexTupleField(last_ret_val, return_index,
                            prepared.xls_func->return_value_count, op_loc);
      // This has (addr, value)
      xls::BValue send_tuple = pb.TupleIndex(send_tup, 0, op_loc);
      xls::BValue condition = pb.TupleIndex(
          send_tup, 1, op_loc,
          absl::StrFormat("%s_pred", bundle_ptr->write_request->name()));

      // This has (addr, value, mask)
      xls::BValue addr = pb.TupleIndex(send_tuple, 0, op_loc);
      xls::BValue value = pb.TupleIndex(send_tuple, 1, op_loc);
      // TODO(google/xls#861): supported masked memory operations.
      xls::BValue mask = pb.Literal(xls::Value::Tuple({}), op_loc);
      xls::BValue send_tuple_with_mask = pb.Tuple({addr, value, mask}, op_loc);
      new_token = pb.SendIf(bundle_ptr->write_request, before_token, condition,
                            send_tuple_with_mask, op_loc);

      xls::BValue receive = pb.ReceiveIf(bundle_ptr->write_response, new_token,
                                         condition, op_loc);
      new_token = pb.TupleIndex(receive, 0);
      // Ignore received value, should be an empty tuple
    } else if (op.op == OpType::kTrace) {
      xls::BValue trace_out_value =
          GetFlexTupleField(last_ret_val, return_index,
                            prepared.xls_func->return_value_count, op_loc);
      XLS_ASSIGN_OR_RETURN(
          new_token, GenerateTrace(trace_out_value, before_token, op, pb));
    } else {
      XLS_CHECK("Unknown IOOp type" == nullptr);
    }
    return new_token;
  };

  // ASAP ops before
  std::vector<xls::BValue> asap_befores_ordered;
  for (const IOOp& op : prepared.xls_func->io_ops) {
    if (op.scheduling_option != IOSchedulingOption::kASAPBefore) {
      continue;
    }

    xls::BValue new_token;
    xls::BValue before_token = prepared.token;
    XLS_ASSIGN_OR_RETURN(new_token, generate_op_invoke(op, before_token));

    asap_befores_ordered.push_back(new_token);
  }
  std::optional<xls::BValue> asap_befores_token;
  if (!asap_befores_ordered.empty()) {
    asap_befores_token = pb.AfterAll(asap_befores_ordered, body_loc);
  }

  // ASAP ops after
  std::vector<xls::BValue> asap_afters_ordered;
  for (const IOOp& op : prepared.xls_func->io_ops) {
    if (op.scheduling_option != IOSchedulingOption::kASAPAfter) {
      continue;
    }

    xls::BValue new_token;
    xls::BValue before_token = asap_befores_token.has_value()
                                   ? asap_befores_token.value()
                                   : prepared.token;
    XLS_ASSIGN_OR_RETURN(new_token, generate_op_invoke(op, before_token));

    asap_afters_ordered.push_back(new_token);
  }

  std::optional<xls::BValue> asap_afters_token;
  if (!asap_afters_ordered.empty()) {
    asap_afters_token = pb.AfterAll(asap_afters_ordered, body_loc);
  }

  std::vector<xls::BValue> fan_ins_tokens;

  // Add the ASAP after token if present, otherwise the before token if present
  if (asap_befores_token.has_value() && !asap_afters_token.has_value()) {
    fan_ins_tokens.push_back(asap_befores_token.value());
  }
  if (asap_afters_token.has_value()) {
    fan_ins_tokens.push_back(asap_afters_token.value());
  }

  absl::flat_hash_map<const IOOp*, xls::BValue> op_tokens;

  // Then default (possibly serialized) ops
  for (const IOOp& op : prepared.xls_func->io_ops) {
    if (op.scheduling_option != IOSchedulingOption::kNone) {
      continue;
    }

    xls::SourceInfo op_loc = op.op_location;

    xls::BValue before_token = prepared.token;
    xls::BValue new_token;
    if (!op.after_ops.empty()) {
      XLSCC_CHECK(op.scheduling_option == IOSchedulingOption::kNone, op_loc);

      std::vector<xls::BValue> after_tokens;
      after_tokens.reserve(op.after_ops.size());
      for (const IOOp* after_op : op.after_ops) {
        XLSCC_CHECK(after_op->scheduling_option == IOSchedulingOption::kNone,
                    op_loc);
        XLSCC_CHECK_NE(&op, after_op, op_loc);
        after_tokens.push_back(op_tokens.at(after_op));
      }
      before_token = pb.AfterAll(after_tokens, body_loc);
    }

    XLS_ASSIGN_OR_RETURN(new_token, generate_op_invoke(op, before_token));

    XLS_CHECK(!op_tokens.contains(&op));
    op_tokens[&op] = new_token;

    fan_ins_tokens.push_back(new_token);
  }

  if (!fan_ins_tokens.empty()) {
    prepared.token = pb.AfterAll(fan_ins_tokens, body_loc);
  }

  return last_ret_val;
}

absl::StatusOr<xls::BValue> Translator::GenerateTrace(
    xls::BValue trace_out_value, xls::BValue before_token, const IOOp& op,
    xls::ProcBuilder& pb) {
  switch (op.trace_type) {
    case TraceType::kNull:
      break;
    case TraceType::kTrace: {
      // Tuple is (condition, ... args ...)
      const uint64_t tuple_count =
          trace_out_value.GetType()->AsTupleOrDie()->size();
      XLS_CHECK_GE(tuple_count, 1);
      xls::BValue condition = pb.TupleIndex(trace_out_value, 0, op.op_location);
      std::vector<xls::BValue> args;
      for (int tuple_idx = 1; tuple_idx < tuple_count; ++tuple_idx) {
        xls::BValue arg =
            pb.TupleIndex(trace_out_value, tuple_idx, op.op_location);
        args.push_back(arg);
      }
      return pb.Trace(before_token, /*condition=*/condition, args,
                      /*message=*/op.trace_message_string, op.op_location);
    }
    case TraceType::kAssert: {
      // Assert condition is !fire
      return pb.Assert(before_token,
                       /*condition=*/pb.Not(trace_out_value, op.op_location),
                       /*message=*/op.trace_message_string,
                       /*label=*/op.label_string.empty()
                           ? std::nullopt
                           : std::optional<std::string>(op.label_string),
                       op.op_location);
    }
  }
  return absl::InternalError(
      ErrorMessage(op.op_location, "Unknown trace type %i", op.trace_type));
}

absl::Status Translator::GenerateIRBlockCheck(
    const HLSBlock& block, const std::list<ExternalChannelInfo>& top_decls,
    const xls::SourceInfo& body_loc) {
  if (!block.has_name()) {
    return absl::InvalidArgumentError(absl::StrFormat("HLSBlock has no name"));
  }

  absl::flat_hash_set<string> channel_names_in_block;
  for (const HLSChannel& channel : block.channels()) {
    if (!channel.has_name() || !channel.has_type() ||
        !(channel.has_is_input() || channel.has_depth())) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Channel is incomplete in proto"));
    }

    channel_names_in_block.insert(channel.name());
  }

  if (top_decls.size() != block.channels_size()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Top function has %i parameters, but block proto defines %i channels",
        top_decls.size(), block.channels_size()));
  }
  for (const ExternalChannelInfo& top_decl : top_decls) {
    const clang::NamedDecl* decl = top_decl.decl;

    if (!channel_names_in_block.contains(decl->getNameAsString())) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Block proto does not contain channels '%s' in function prototype",
          decl->getNameAsString()));
    }
    channel_names_in_block.erase(decl->getNameAsString());
  }

  if (!channel_names_in_block.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Block proto contains %i channels not in function prototype",
        channel_names_in_block.size()));
  }

  return absl::OkStatus();
}

absl::StatusOr<CValue> Translator::GenerateTopClassInitValue(
    const std::shared_ptr<CType>& this_type,
    // Can be nullptr
    const clang::CXXRecordDecl* this_decl, const xls::SourceInfo& body_loc) {
  for (const clang::CXXConstructorDecl* ctor : this_decl->ctors()) {
    if (!(ctor->isTrivial() || ctor->isDefaultConstructor())) {
      ctor->dump();
      return absl::UnimplementedError(ErrorMessage(
          body_loc, "Non-trivial constructors in top class not yet supported"));
    }
  }

  XLS_ASSIGN_OR_RETURN(std::shared_ptr<CType> resolved_type,
                       ResolveTypeInstance(this_type));

  auto struct_type = std::dynamic_pointer_cast<CStructType>(resolved_type);
  XLS_CHECK_NE(struct_type, nullptr);

  PushContextGuard temporary_this_context(*this, body_loc);

  // Don't allow "this" to be propagated up: it's only temporary for use
  // within the initializer list
  context().propagate_up = false;
  context().override_this_decl_ = this_decl;
  context().ast_context = &this_decl->getASTContext();

  XLS_ASSIGN_OR_RETURN(CValue this_val,
                       CreateDefaultCValue(this_type, body_loc));

  XLS_RETURN_IF_ERROR(DeclareVariable(this_decl, this_val, body_loc));

  // Check for side-effects
  for (const clang::FieldDecl* field_decl : this_decl->fields()) {
    std::shared_ptr<CField> field = struct_type->get_field(field_decl);
    std::shared_ptr<CType> field_type = field->type();
    auto field_decl_loc = GetLoc(*field_decl);

    CValue field_val;
    if (!field_decl->hasInClassInitializer()) {
      XLS_ASSIGN_OR_RETURN(field_val,
                           CreateDefaultCValue(field_type, field_decl_loc));
    } else {
      PushContextGuard guard(*this, field_decl_loc);
      context().any_side_effects_requested = false;

      XLS_ASSIGN_OR_RETURN(
          field_val,
          GenerateIR_Expr(field_decl->getInClassInitializer(), field_decl_loc));

      if (context().any_side_effects_requested) {
        return absl::UnimplementedError(
            ErrorMessage(field_decl_loc,
                         "Side effects in initializer for top class field %s",
                         field_decl->getQualifiedNameAsString()));
      }
    }

    XLS_CHECK(*field_val.type() == *field_type);

    {
      UnmaskAndIgnoreSideEffectsGuard unmask_guard(*this);
      XLS_RETURN_IF_ERROR(
          AssignMember(this_decl, field_decl, field_val, field_decl_loc));
    }
  }

  XLS_ASSIGN_OR_RETURN(this_val, GetIdentifier(this_decl, body_loc));

  return this_val;
}

absl::Status Translator::GenerateIRBlockPrepare(
    PreparedBlock& prepared, xls::ProcBuilder& pb, int64_t next_return_index,
    int64_t next_state_index, std::shared_ptr<CType> this_type,
    const clang::CXXRecordDecl* this_decl,
    const std::list<ExternalChannelInfo>& top_decls,
    const xls::SourceInfo& body_loc) {
  // For defaults, updates, invokes
  GeneratedFunction temp_sf;

  XLSCC_CHECK(!context_stack_.empty(), body_loc);
  context() = TranslationContext();
  context().propagate_up = false;
  context().sf = &temp_sf;
  context().fb = dynamic_cast<xls::BuilderBase*>(&pb);

  // This state and argument
  if (this_decl != nullptr) {
    XLS_ASSIGN_OR_RETURN(CValue this_cval, GenerateTopClassInitValue(
                                               this_type, this_decl, body_loc));

    XLS_CHECK(this_cval.rvalue().valid());
    XLS_ASSIGN_OR_RETURN(xls::Value this_init_val,
                         EvaluateBVal(this_cval.rvalue(), body_loc));

    prepared.state_index_for_static[this_decl] = next_state_index++;
    pb.StateElement("this", this_init_val, body_loc);
    ++prepared.state_init_count;

    prepared.args.push_back(
        pb.GetStateParam(prepared.state_index_for_static.at(this_decl)));
  }

  for (const clang::NamedDecl* namedecl :
       prepared.xls_func->GetDeterministicallyOrderedStaticValues()) {
    const ConstValue& initval = prepared.xls_func->static_values.at(namedecl);

    pb.StateElement(XLSNameMangle(clang::GlobalDecl(namedecl)),
                    initval.rvalue(), body_loc);
    ++prepared.state_init_count;
  }

  // Add returns for static locals
  {
    for (const clang::NamedDecl* namedecl :
         prepared.xls_func->GetDeterministicallyOrderedStaticValues()) {
      prepared.return_index_for_static[namedecl] = next_return_index++;
      prepared.state_index_for_static[namedecl] = next_state_index++;
    }
  }

  // This return
  if (this_decl != nullptr) {
    prepared.return_index_for_static[this_decl] = next_return_index++;
  }

  // Prepare direct-ins
  for (const ExternalChannelInfo& top_decl : top_decls) {
    if (top_decl.interface_type != InterfaceType::kDirect) {
      continue;
    }

    const ChannelBundle& bundle = top_decl.external_channels;
    xls::Channel* xls_channel = bundle.regular;
    unused_external_channels_.remove(xls_channel);

    xls::BValue receive = pb.Receive(xls_channel, prepared.token);
    prepared.token = pb.TupleIndex(receive, 0);
    xls::BValue direct_in_value = pb.TupleIndex(receive, 1);

    prepared.args.push_back(direct_in_value);

    // If it's const or not a reference, then there's no return
    if (top_decl.extra_return) {
      ++next_return_index;
    }
  }

  // Initialize parameters to defaults, handle direct-ins, create channels
  // Add channels in order of function prototype
  // Find return indices for ops
  for (const IOOp& op : prepared.xls_func->io_ops) {
    prepared.return_index_for_op[&op] = next_return_index++;

    if (op.op == OpType::kTrace) {
      continue;
    }

    if (op.channel->generated != nullptr) {
      ChannelBundle generated_bundle = {.regular = op.channel->generated};
      prepared.xls_channel_by_function_channel[op.channel] = generated_bundle;
      continue;
    }

    if (!prepared.xls_channel_by_function_channel.contains(op.channel)) {
      XLSCC_CHECK(external_channels_by_internal_channel_.contains(op.channel),
                  body_loc);
      XLSCC_CHECK_EQ(external_channels_by_internal_channel_.count(op.channel),
                     1, body_loc);
      const ChannelBundle bundle =
          external_channels_by_internal_channel_.find(op.channel)->second;
      prepared.xls_channel_by_function_channel[op.channel] = bundle;
    }
  }

  // Params
  for (const xlscc::SideEffectingParameter& param :
       prepared.xls_func->side_effecting_parameters) {
    switch (param.type) {
      case xlscc::SideEffectingParameterType::kIOOp: {
        const IOOp& op = *param.io_op;
        if (op.op == OpType::kRecv) {
          XLS_ASSIGN_OR_RETURN(
              xls::BValue val,
              CreateDefaultValue(op.channel->item_type, body_loc));
          if (!op.is_blocking) {
            val = pb.Tuple({val, pb.Literal(xls::UBits(1, 1), body_loc)},
                           body_loc);
          }
          prepared.arg_index_for_op[&op] = prepared.args.size();
          prepared.args.push_back(val);
        } else if (op.op == OpType::kRead) {
          XLS_ASSIGN_OR_RETURN(
              xls::BValue default_value,
              CreateDefaultValue(op.channel->item_type, body_loc));
          prepared.arg_index_for_op[&op] = prepared.args.size();
          prepared.args.push_back(default_value);
        }
        break;
      }
      case xlscc::SideEffectingParameterType::kStatic: {
        const uint64_t static_idx =
            prepared.state_index_for_static.at(param.static_value);
        prepared.args.push_back(pb.GetStateParam(static_idx));
        break;
      }
      default: {
        return absl::InternalError(
            ErrorMessage(body_loc, "Unknown type of SideEffectingParameter"));
        break;
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace xlscc
