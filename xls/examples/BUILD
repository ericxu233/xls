# Copyright 2020 The XLS Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Build rules for XLS examples.

load("//xls/examples:list_filegroup_files.bzl", "list_filegroup_files")
load(
    "//xls/build_rules:xls_build_defs.bzl",
    "cc_xls_ir_jit_wrapper",
    "xls_benchmark_ir",
    "xls_dslx_ir",
    "xls_dslx_library",
    "xls_dslx_opt_ir",
    "xls_dslx_opt_ir_test",
    "xls_dslx_test",
    "xls_dslx_verilog",
    "xls_eval_ir_test",
    "xls_ir_opt_ir",
    "xls_ir_verilog",
)
load("@rules_hdl//verilog:providers.bzl", "verilog_library")
load("@rules_hdl//synthesis:build_defs.bzl", "synthesize_rtl")
load("@rules_hdl//place_and_route:build_defs.bzl", "place_and_route")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//xls:xls_internal"],
    licenses = ["notice"],
)

exports_files(["sha256.x"])

filegroup(
    name = "ir_examples",
    srcs = [
        ":adler32.ir",
        ":adler32.opt.ir",
        ":crc32.ir",
        ":crc32.opt.ir",
        ":matmul_4x4.ir",
        ":sha256.ir",
        ":sha256.opt.ir",
    ],
)

xls_dslx_opt_ir(
    name = "adler32",
    srcs = ["adler32.x"],
    dslx_top = "main",
    ir_file = "adler32.ir",
    opt_ir_file = "adler32.opt.ir",
)

xls_dslx_test(
    name = "adler32_dslx_test",
    srcs = ["adler32.x"],
)

# TODO(rspringer): OOMs. Add xls_ir_equivalence_test.
xls_eval_ir_test(
    name = "adler32_eval_ir_test",
    src = ":adler32.ir",
)

xls_benchmark_ir(
    name = "adler32_benchmark_ir",
    src = ":adler32.ir",
    benchmark_ir_args = {
        "delay_model": "sky130",
    },
)

cc_binary(
    name = "adler32_reference",
    srcs = ["adler32_reference.cc"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/types:span",
        "//xls/common:init_xls",
        "//xls/common/logging",
    ],
)

xls_dslx_opt_ir(
    name = "capitalize",
    srcs = ["capitalize.x"],
    dslx_top = "main",
)

xls_dslx_opt_ir_test(
    name = "capitalize_test",
    dep = ":capitalize",
)

xls_dslx_opt_ir(
    name = "crc32",
    srcs = ["crc32.x"],
    dslx_top = "main",
    ir_file = "crc32.ir",
    opt_ir_file = "crc32.opt.ir",
)

xls_benchmark_ir(
    name = "crc32_benchmark_ir",
    src = ":crc32.ir",
    tags = ["optonly"],
)

xls_dslx_opt_ir_test(
    name = "crc32_test",
    dep = ":crc32",
)

cc_xls_ir_jit_wrapper(
    name = "crc32_jit_wrapper",
    src = ":crc32.opt.ir",
    jit_wrapper_args = {
        "class_name": "crc32",
        "namespace": "xls::examples",
    },
)

cc_binary(
    name = "crc32_reference",
    srcs = ["crc32_reference.cc"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/types:span",
        "//xls/common:init_xls",
        "//xls/common/logging",
    ],
)

xls_dslx_library(
    name = "dot_product_dslx",
    srcs = ["dot_product.x"],
)

xls_dslx_test(
    name = "dot_product_dslx_test",
    srcs = ["dot_product.x"],
    deps = [":dot_product_dslx"],
)

xls_dslx_library(
    name = "fir_filter_dslx",
    srcs = ["fir_filter.x"],
)

xls_dslx_test(
    name = "fir_filter_dslx_test",
    srcs = ["fir_filter.x"],
    deps = [":fir_filter_dslx"],
)

xls_dslx_test(
    name = "matmul_4x4_dslx_test",
    srcs = ["matmul_4x4.x"],
    # TODO(rspringer): 2022-05-24: Does not successfully lower to IR.
    # Needs support for arrays of channels.
)

xls_dslx_opt_ir(
    name = "riscv_simple",
    srcs = ["riscv_simple.x"],
    dslx_top = "run_instruction",
)

xls_benchmark_ir(
    name = "riscv_simple_benchmark_ir",
    src = ":riscv_simple.ir",
    tags = ["optonly"],
)

xls_dslx_library(
    name = "ffi",
    srcs = ["ffi.x"],
)

xls_dslx_test(
    name = "ffi_test",
    srcs = ["ffi.x"],
)

xls_dslx_verilog(
    name = "ffi_codegen",
    codegen_args = {
        "module_name": "ffi",
        "generator": "pipeline",
        "pipeline_stages": "1",
        "delay_model": "sky130",
        "ffi_fallback_delay_ps": "123",
        "use_system_verilog": "true",
    },
    dslx_top = "main",
    library = ":ffi",
    verilog_file = "ffi.sv",
)

# TODO: 2021-05-28 Add xls_ir_equivalence_test, xls_eval_ir_test and
# xls_benchmark_ir.
xls_dslx_test(
    name = "riscv_simple_dslx_test",
    srcs = ["riscv_simple.x"],
    dslx_test_args = {
        # TODO(hjmontero): run_instruction segfaults in the JIT.
        "compare": "interpreter",
    },
)

xls_dslx_opt_ir(
    name = "sha256",
    srcs = ["sha256.x"],
    dslx_top = "main",
    ir_file = "sha256.ir",
    opt_ir_file = "sha256.opt.ir",
    tags = ["optonly"],
)

xls_dslx_test(
    name = "sha256_dslx_test",
    srcs = ["sha256.x"],
)

# TODO(rspringer): OOMs on some machines. Add xls_ir_equivalence_test.
xls_eval_ir_test(
    name = "sha256_eval_ir_test",
    src = ":sha256.ir",
    tags = ["optonly"],
)

xls_benchmark_ir(
    name = "sha256_benchmark_ir",
    src = ":sha256.ir",
    tags = ["optonly"],
)

cc_xls_ir_jit_wrapper(
    name = "sha256_jit_wrapper",
    src = ":sha256.opt.ir",
    jit_wrapper_args = {
        "class_name": "sha256",
        "function": "main",
        "namespace": "xls::examples",
    },
)

xls_dslx_library(
    name = "sobel_filter_dslx",
    srcs = ["sobel_filter.x"],
    deps = [
        "//third_party/xls_go_math:fpsqrt_32_dslx",
    ],
)

xls_dslx_test(
    name = "sobel_filter_test",
    srcs = ["sobel_filter.x"],
    deps = [":sobel_filter_dslx"],
)

xls_dslx_opt_ir(
    name = "sobel_filter_ir",
    srcs = ["sobel_filter_benchmark.x"],
    dslx_top = "apply_stencil_float32_8x8",
    deps = [":sobel_filter_dslx"],
)

xls_benchmark_ir(
    name = "sobel_filter_benchmark_ir",
    src = ":sobel_filter_ir.ir",
)

list_filegroup_files(
    name = "ir_example_file_list",
    src = ":ir_examples",
    out = "ir_example_file_list.txt",
)

cc_library(
    name = "sample_packages",
    srcs = [
        "sample_packages.cc",
        "sample_packages.inc.h",
    ],
    hdrs = ["sample_packages.h"],
    data = [
        ":ir_example_file_list.txt",
        ":ir_examples",
        "//xls/examples:adler32.opt.ir",
    ],
    deps = [
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "//xls/common/file:filesystem",
        "//xls/common/file:get_runfile_path",
        "//xls/common/file:path",
        "//xls/common/logging",
        "//xls/common/status:status_macros",
        "//xls/ir",
        "//xls/ir:function_builder",
        "//xls/ir:ir_parser",
    ],
)

cc_library(
    name = "proc_fir_filter",
    srcs = ["proc_fir_filter.cc"],
    hdrs = ["proc_fir_filter.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "//xls/common/status:status_macros",
        "//xls/ir",
        "//xls/ir:bits",
        "//xls/ir:channel",
        "//xls/ir:function_builder",
        "//xls/ir:node_util",
        "//xls/ir:op",
        "//xls/ir:source_location",
        "//xls/ir:value_helpers",
    ],
)

cc_test(
    name = "matmul_4x4_test",
    srcs = ["matmul_4x4_test.cc"],
    data = ["matmul_4x4.ir"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "//xls/common:xls_gunit",
        "//xls/common:xls_gunit_main",
        "//xls/common/file:filesystem",
        "//xls/common/file:get_runfile_path",
        "//xls/common/status:matchers",
        "//xls/interpreter:channel_queue",
        "//xls/interpreter:interpreter_proc_runtime",
        "//xls/interpreter:serial_proc_runtime",
        "//xls/ir:bits",
        "//xls/ir:ir_parser",
        "//xls/ir:value",
    ],
)

cc_test(
    name = "proc_fir_filter_test",
    srcs = ["proc_fir_filter_test.cc"],
    deps = [
        ":proc_fir_filter",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "//xls/common:xls_gunit",
        "//xls/common:xls_gunit_main",
        "//xls/common/status:matchers",
        "//xls/common/status:status_macros",
        "//xls/interpreter:channel_queue",
        "//xls/interpreter:interpreter_proc_runtime",
        "//xls/interpreter:serial_proc_runtime",
        "//xls/ir",
        "//xls/ir:bits",
        "//xls/ir:ir_test_base",
    ],
)

xls_dslx_opt_ir(
    name = "proc_iota",
    srcs = ["proc_iota.x"],
    dslx_top = "main",
    ir_file = "proc_iota.ir",
    opt_ir_file = "proc_iota.opt.ir",
    tags = ["optonly"],
)

# -- tiny_adder sample

xls_dslx_library(
    name = "tiny_adder_dslx",
    srcs = ["tiny_adder.x"],
)

xls_dslx_test(
    name = "tiny_adder_test",
    library = ":tiny_adder_dslx",
)

xls_dslx_opt_ir(
    name = "tiny_adder_opt_ir",
    srcs = ["tiny_adder.x"],
    dslx_top = "tiny_adder",
    ir_file = "tiny_adder.ir",
    opt_ir_file = "tiny_adder.opt.ir",
)

# -- find_index sample, with OpenROAD synthesis

xls_dslx_library(
    name = "find_index_dslx",
    srcs = ["find_index.x"],
)

xls_dslx_test(
    name = "find_index_test",
    library = ":find_index_dslx",
)

xls_dslx_opt_ir(
    name = "find_index_opt_ir",
    srcs = ["find_index.x"],
    dslx_top = "find_index",
    ir_file = "find_index.ir",
    opt_ir_file = "find_index.opt.ir",
)

xls_dslx_verilog(
    name = "find_index_5000ps_model_unit",
    codegen_args = {
        "delay_model": "unit",
        "clock_period_ps": "5000",
        "reset_data_path": "false",
        "module_name": "find_index",
        "use_system_verilog": "false",
    },
    dslx_top = "find_index",
    library = ":find_index_dslx",
    verilog_file = "find_index_5000ps_model_unit.v",
)

verilog_library(
    name = "find_index_5000ps_model_unit_verilog",
    srcs = [
        ":find_index_5000ps_model_unit.v",
    ],
)

synthesize_rtl(
    name = "find_index_5000ps_model_unit_verilog_synth",
    top_module = "find_index",
    deps = [
        ":find_index_5000ps_model_unit_verilog",
    ],
)

synthesize_rtl(
    name = "find_index_5000ps_model_unit_verilog_synth_by_stage",
    synth_tcl = "//xls/synthesis/yosys:synth_by_stage.tcl",
    top_module = "find_index",
    deps = [
        ":find_index_5000ps_model_unit_verilog",
    ],
)

xls_dslx_opt_ir(
    name = "large_array",
    srcs = ["large_array.x"],
    dslx_top = "large_array",
    ir_file = "large_array.ir",
    opt_ir_file = "large_array.opt.ir",
)

xls_benchmark_ir(
    name = "large_array_benchmark_ir",
    src = ":large_array.ir",
)

xls_dslx_library(
    name = "ram_dslx",
    srcs = ["ram.x"],
)

xls_dslx_test(
    name = "ram_test",
    library = ":ram_dslx",
)

xls_dslx_library(
    name = "delay_dslx",
    srcs = ["delay.x"],
    deps = [":ram_dslx"],
)

xls_dslx_test(
    name = "delay_test",
    library = ":delay_dslx",
)

xls_dslx_ir(
    name = "delay_ir",
    dslx_top = "Delay32x2048_init3",
    ir_file = "delay.ir",
    library = ":delay_dslx",
)

xls_ir_opt_ir(
    name = "delay_opt_ir",
    src = "delay.ir",
    top = "__delay__Delay32x2048_init3__Delay__DelayInternal_0__10_32_2048_64_1024_3_next",
)

xls_ir_verilog(
    name = "delay_sv",
    src = ":delay_opt_ir.opt.ir",
    codegen_args = {
        "module_name": "delay_top",
        "generator": "pipeline",
        "delay_model": "unit",
        # Enable codegen pass that rewrites the req and resp channels to drive
        # an external RAM. The configuration below means that the pair of
        # channels `delay__ram_req` and `delay__ram_resp` drive a 1RW RAM
        # and output ports should be prefixed with `ram`, e.g. `ram_addr`.
        # See https://google.github.io/xls/codegen_options/#rams-experimental
        # for more information about this option.
        "ram_configurations": "ram:1RW:{req}:{resp}:{wr_comp}".format(
            req = "delay__ram_req",
            resp = "delay__ram_resp",
            wr_comp = "delay__ram_wr_comp",
        ),
        "pipeline_stages": "2",
        "reset": "rst",
        "reset_data_path": "true",
        "reset_active_low": "false",
        "reset_asynchronous": "true",
        "flop_inputs": "false",
        "flop_single_value_channels": "false",
        "flop_outputs": "false",
        "add_idle_output": "false",
        "streaming_channel_data_suffix": "_data",
        "streaming_channel_ready_suffix": "_ready",
        "streaming_channel_valid_suffix": "_valid",
        "use_system_verilog": "true",
    },
    verilog_file = "delay.sv",
)

verilog_library(
    name = "delay",
    srcs = [":delay.sv"],
)

xls_dslx_library(
    name = "quickcheck_dslx",
    srcs = ["quickcheck.x"],
)

xls_dslx_test(
    name = "quickcheck_test",
    dslx_test_args = {
        "compare": "jit",
    },
    library = ":quickcheck_dslx",
)

xls_dslx_library(
    name = "constraint_dslx",
    srcs = ["constraint.x"],
)

xls_dslx_ir(
    name = "constraint_ir",
    dslx_top = "main",
    ir_file = "constraint.ir",
    library = ":constraint_dslx",
)

xls_ir_opt_ir(
    name = "constraint_opt_ir",
    src = "constraint.ir",
)

xls_ir_verilog(
    name = "constraint_sv",
    src = ":constraint_opt_ir.opt.ir",
    codegen_args = {
        "module_name": "constraint_top",
        "generator": "pipeline",
        "delay_model": "unit",
        # Set a constraint between req and resp that they must be 2 cycles apart
        # See https://google.github.io/xls/codegen_options/#pipelining-and-scheduling-options
        # for more information about this option.
        "io_constraints": "constraint__req:send:constraint__resp:recv:2:2",
        "pipeline_stages": "4",
        "clock_period_ps": "2",
        "reset": "rst",
        "use_system_verilog": "true",
    },
    verilog_file = "constraint.sv",
)

verilog_library(
    name = "constraint",
    srcs = [":constraint.sv"],
)

place_and_route(
    name = "find_index_place_and_route",
    # ~64 MhZ
    clock_period = "15.42857143",
    core_padding_microns = 30,
    placement_density = "0.8",
    synthesized_rtl = ":find_index_5000ps_model_unit_verilog_synth",
    target_die_utilization_percentage = "30",
)

xls_dslx_library(
    name = "apfloat_fmac_dslx",
    srcs = ["apfloat_fmac.x"],
)

xls_dslx_library(
    name = "fp32_fmac_dslx",
    srcs = ["fp32_fmac.x"],
    deps = [":apfloat_fmac_dslx"],
)

xls_dslx_opt_ir(
    name = "fp32_fmac",
    dslx_top = "fp32_fmac",
    library = ":fp32_fmac_dslx",
)

xls_dslx_test(
    name = "fp32_fmac_test",
    dslx_test_args = {
        "compare": "none",
    },
    library = ":fp32_fmac_dslx",
)

xls_dslx_opt_ir(
    name = "overflow_detect",
    srcs = ["overflow_detect.x"],
    dslx_top = "main",
    ir_file = "overflow_detect.ir",
    opt_ir_file = "overflow_detect.opt.ir",
)

xls_dslx_opt_ir(
    name = "nested_sel",
    srcs = ["nested_sel.x"],
    dslx_top = "main",
)

xls_dslx_opt_ir_test(
    name = "nested_sel_test",
    dep = ":nested_sel",
)

filegroup(
    name = "x_files",
    srcs = glob(["*.x"]),
    visibility = ["//xls:xls_internal"],
)
