/*
* Copyright (C) 1999-2017 John K�ll�n.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "stdafx.h"

#include "reko.h"

#include "functions.h"
#include "ComBase.h"
#include "NativeInstruction.h"
#include "ThumbDisassembler.h"
#include "ArmArchitecture.h"

ThumbDisassembler::ThumbDisassembler(const uint8_t * bytes, size_t length, int offset, uint64_t uAddr) :
	bytes(bytes), length(length), offset(offset), uAddr(uAddr)
{
	auto ec = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &hcapstone);
	ec = cs_option(this->hcapstone, CS_OPT_DETAIL, CS_OPT_ON);
}

ThumbDisassembler::~ThumbDisassembler()
{
	Dump("Destroying ThumbDisassembler");
}

HRESULT STDAPICALLTYPE ThumbDisassembler::QueryInterface(REFIID iid, void ** ppvOut)
{
	if (iid == IID_INativeDisassembler ||
		iid == IID_IAgileObject ||
		iid == IID_IUnknown)
	{
		AddRef();
		*ppvOut = static_cast<INativeDisassembler *>(this);
		return S_OK;
	}
	ppvOut = nullptr;
	return E_NOINTERFACE;
}

INativeInstruction * ThumbDisassembler::NextInstruction()
{
	if (length == 0)
	{
		return false;
	}
	uint64_t uAddr = this->uAddr;
	auto instr = cs_malloc(hcapstone);
	if (!cs_disasm_iter(hcapstone, &this->bytes, &this->length, &this->uAddr, instr))
	{
		auto info = NativeInstructionInfo{
			uAddr, 4, static_cast<uint32_t>(InstructionClass::Invalid), ARM_INS_INVALID
		};
		this->uAddr += 4;
		this->length -= 4;
		return new NativeInstruction(instr, info);
	}
	else
	{
		auto info = NativeInstructionInfo{
			uAddr, 4, static_cast<uint32_t>(InstructionClass::Linear), instr->id
		};
		return new NativeInstruction(instr, info);
	}
}

inline InstructionClass operator | (InstructionClass a, InstructionClass b) {
	return static_cast<InstructionClass>(static_cast<int>(a) | static_cast<int>(b));
}

InstructionClass ThumbDisassembler::InstructionClassFromId(unsigned int armInstrID)
{
	switch (armInstrID)
	{
	case ARM_INS_INVALID: return InstructionClass::Invalid;
	case ARM_INS_BKPT: return InstructionClass::Transfer;
	case ARM_INS_BL: return InstructionClass::Transfer | InstructionClass::Call;
	case ARM_INS_BLX: return InstructionClass::Transfer | InstructionClass::Call;
	case ARM_INS_BX: return InstructionClass::Transfer;
	case ARM_INS_BXJ: return InstructionClass::Transfer;
	case ARM_INS_B: return InstructionClass::Transfer;
	case ARM_INS_HLT: return InstructionClass::Transfer;
	case ARM_INS_SVC: return InstructionClass::Transfer;
	case ARM_INS_TEQ: return InstructionClass::Transfer;
	case ARM_INS_TRAP: return InstructionClass::Transfer;
	case ARM_INS_YIELD: return InstructionClass::Transfer;
	}
	return InstructionClass::Linear;
}

