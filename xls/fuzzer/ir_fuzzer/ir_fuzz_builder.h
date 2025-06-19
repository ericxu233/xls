// Copyright 2025 The XLS Authors
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

#ifndef XLS_FUZZER_IR_FUZZER_IR_FUZZ_BUILDER_H_
#define XLS_FUZZER_IR_FUZZER_IR_FUZZ_BUILDER_H_

#include <string>
#include <vector>

#include "xls/fuzzer/ir_fuzzer/fuzz_program.pb.h"
#include "xls/ir/function_builder.h"
#include "xls/ir/node.h"
#include "xls/ir/package.h"

namespace xls {

// Class used to convert randomly generated objects into a valid IR object. More
// specifically, the conversion from a FuzzProgramProto to an IR BValue. It uses
// a stack to maintain a list of BValues, where each element represents a node
// in the IR that was instantiated/created from a FuzzOpProto. A stack is used
// to allow new FuzzOpProtos to retrieve previous BValues from the stack without
// using explicit references like the IR does. We cannot use explicit references
// because randomly generated FuzzOpProtos would lead to many invalid IR ops,
// which adds complexity.
class IrFuzzBuilder {
 public:
  IrFuzzBuilder(Package* package, FunctionBuilder* function_builder,
                const FuzzProgramProto& fuzz_program);
  BValue BuildIr();
  std::string PrintProgram();

 private:
  void InstFuzzOps();
  BValue CombineStack();

  // Package and FunctionBuilder are used to create new IR BValues.
  Package* p_;
  FunctionBuilder* fb_;
  // FuzzProgramProto is randomly generated instructions based off a protobuf
  // structure used to instantiate IR BValues.
  FuzzProgramProto fuzz_program_;
  // Stack is used to maintain a list of BValues that are created from the
  // FuzzProgramProto.
  std::vector<BValue> stack_;
};

}  // namespace xls

#endif  // XLS_FUZZER_IR_FUZZER_IR_FUZZ_BUILDER_H_
