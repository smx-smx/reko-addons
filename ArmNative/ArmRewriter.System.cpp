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

#include "ComBase.h"
#include "ArmRewriter.h"

void ArmRewriter::RewriteCdp()
{
	auto cdp = host->EnsurePseudoProcedure("__cdp", BaseType::Void, instr->detail->arm.op_count);
	auto begin = &instr->detail->arm.operands[0];
	auto end = begin + instr->detail->arm.op_count;
	for (auto op = begin; op != end; ++op)
	{
		m.AddArg(Operand(*op));
	}
	m.SideEffect(m.Fn(cdp));
	
}

void ArmRewriter::RewriteCps()
{
	if (instr->detail->arm.cps_mode == ARM_CPSMODE_ID)
	{
		m.SideEffect(m.Fn(host->EnsurePseudoProcedure("__cps_id", BaseType::Void, 1)));
		return;
	}
	NotImplementedYet();
}

void ArmRewriter::RewriteDmb()
{
	auto memBarrier = MemBarrierName(instr->detail->arm.mem_barrier);
	char name[100];
	snprintf(name, sizeof(name), "__dmb_%s", memBarrier);
	m.SideEffect(m.Fn(host->EnsurePseudoProcedure(name, BaseType::Void, 1)));
}

void ArmRewriter::RewriteMcr()
{
	auto begin = &instr->detail->arm.operands[0];
	auto end = begin + instr->detail->arm.op_count;
	int cArgs = 0;
	for (auto op = begin; op != end; ++op)
	{
		m.AddArg(Operand(*op));
		++cArgs;
	}
	auto ppp = host->EnsurePseudoProcedure("__mcr", BaseType::Void, cArgs);
	m.SideEffect(m.Fn(ppp));
}

void ArmRewriter::RewriteMrc()
{
	auto begin = &instr->detail->arm.operands[0];
	auto end = begin + instr->detail->arm.op_count;
	int cArgs = 0;
	HExpr regDst = HExpr(-1);
	for (auto op = begin; op != end; ++op)
	{
		auto a = Operand(*op);
		if (cArgs == 2)
		{
			regDst = a;
		}
		else
		{
			m.AddArg(a);
		}
		++cArgs;
	}
	auto ppp = host->EnsurePseudoProcedure("__mrc", BaseType::Void, cArgs-1);
	m.Assign(regDst, m.Fn(ppp));
}

void ArmRewriter::RewriteMrs()
{
	auto ppp = host->EnsurePseudoProcedure("__mrs", BaseType::Word32, 1);
	m.AddArg(Operand(Src1()));
	m.Assign(Operand(Dst()), m.Fn(ppp));
}

void ArmRewriter::RewriteMsr()
{
	auto ppp = host->EnsurePseudoProcedure("__msr", BaseType::Word32, 2);
	m.AddArg(Operand(Dst()));
	m.AddArg(Operand(Src1()));
	m.SideEffect(m.Fn(ppp));
}

void ArmRewriter::RewriteStcl()
{
	auto intrinsic = host->EnsurePseudoProcedure("__stcl", BaseType::Word32, 2);
	m.AddArg(Operand(Dst()));
	m.AddArg(Operand(Src1()));
	m.AddArg(Operand(Src2()));
	m.SideEffect(m.Fn(intrinsic));
}

const char * ArmRewriter::MemBarrierName(arm_mem_barrier barrier)
{
	switch (barrier)
	{
		//case ARM_MB_INVALID = 0,
		//case 	ARM_MB_RESERVED_0,
	case 	ARM_MB_OSHLD: return "oshld";
	case 	ARM_MB_OSHST: return "oshst";
	case 	ARM_MB_OSH: return "osh";
		//case 	ARM_MB_RESERVED_4,
	case 	ARM_MB_NSHLD: return "nshld";
	case 	ARM_MB_NSHST: return "nshst";
	case 	ARM_MB_NSH: return "nsh";
		//case 	ARM_MB_RESERVED_8,
	case 	ARM_MB_ISHLD: return "ishld";
	case 	ARM_MB_ISHST: return "ishst";
	case 	ARM_MB_ISH: return "ish";
		//case 	ARM_MB_RESERVED_12,
	case 	ARM_MB_LD: return "ld";
	case 	ARM_MB_ST: return "st";
	case 	ARM_MB_SY: return "sy";
	}
	return "NOT_IMPLEMENTED";

}