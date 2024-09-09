/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include "dynarmic/backend/arm64/address_space.h"
#include "dynarmic/backend/block_range_information.h"
#include "dynarmic/interface/A32/config.h"

namespace Dynarmic::Backend::Arm64 {

struct EmittedBlockInfo;

class A32AddressSpace final : public AddressSpace {
public:
    explicit A32AddressSpace(const A32::UserConfig& conf);

    IR::Block GenerateIR(IR::LocationDescriptor, u64& pc, u32& inst) const override;

    void InvalidateCacheRanges(const boost::icl::interval_set<u32>& ranges);

    void Initialize(u32 _halt_reason_on_run, u64 _trace_scope_begin, u64 _trace_scope_end) {
        halt_reason_on_run = _halt_reason_on_run;
        trace_scope_begin = _trace_scope_begin;
        trace_scope_end = _trace_scope_end;
        EmitPrelude();
    }

protected:
    friend class A32Core;

    void EmitPrelude();
    EmitConfig GetEmitConfig() override;
    void RegisterNewBasicBlock(const IR::Block& block, const EmittedBlockInfo& block_info) override;

    void GenHaltReasonSet(oaknut::Label& run_code_entry);
    void GenHaltReasonSet(oaknut::Label& run_code_entry, oaknut::Label& ret_code_entry);
    void GenHaltReasonSetImpl(bool isRet, oaknut::Label& run_code_entry, oaknut::Label& ret_code_entry);

    const A32::UserConfig conf;
    BlockRangeInformation<u32> block_ranges;

    u32 halt_reason_on_run;
    u64 trace_scope_begin;
    u64 trace_scope_end;
};

}  // namespace Dynarmic::Backend::Arm64
