// Copyright 2021 The XLS Authors
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

#ifndef XLS_CODEGEN_CODEGEN_WRAPPER_PASS_H_
#define XLS_CODEGEN_CODEGEN_WRAPPER_PASS_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "xls/codegen/codegen_pass.h"
#include "xls/ir/package.h"
#include "xls/passes/optimization_pass.h"
#include "xls/passes/pass_base.h"

namespace xls::verilog {

// A codegen pass wrapper which wraps a OptimizationFunctionBasePass. This is
// useful for adding an optimization or transformation pass (most passes in
// xls/passes are OptimizationFunctionBasePasses) to the codegen pipeline. The
// wrapped pass is run on the block being lowered to Verilog.
//
// Takes the OptimizationContext object at construction, since it's specific to
// optimization passes & cannot be passed via a codegen pass's Run function.
class CodegenWrapperPass : public CodegenPass {
 public:
  explicit CodegenWrapperPass(
      std::unique_ptr<OptimizationFunctionBasePass> wrapped_pass,
      OptimizationContext& opt_context)
      : CodegenPass(absl::StrFormat("codegen_%s", wrapped_pass->short_name()),
                    absl::StrFormat("%s (codegen)", wrapped_pass->long_name())),
        wrapped_pass_(std::move(wrapped_pass)),
        opt_context_(opt_context) {}
  ~CodegenWrapperPass() override = default;

  bool IsCompound() const override { return wrapped_pass_->IsCompound(); }

  absl::StatusOr<bool> RunInternal(Package* package,
                                   const CodegenPassOptions& options,
                                   PassResults* results,
                                   CodegenContext& context) const final;

  absl::StatusOr<bool> RunNested(
      Package* package, const CodegenPassOptions& options, PassResults* results,
      CodegenContext& context, PassInvocation& invocation,
      absl::Span<const CodegenInvariantChecker* const> invariant_checkers)
      const override {
    return wrapped_pass_->RunNested(package, OptimizationPassOptions(options),
                                    results, opt_context_, invocation,
                                    /*invariant_checkers=*/{});
  }

 private:
  std::unique_ptr<OptimizationFunctionBasePass> wrapped_pass_;
  OptimizationContext& opt_context_;
};

}  // namespace xls::verilog

#endif  // XLS_CODEGEN_CODEGEN_WRAPPER_PASS_H_
