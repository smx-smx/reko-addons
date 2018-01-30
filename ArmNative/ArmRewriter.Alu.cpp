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

void ArmRewriter::RewriteAdcSbc(BinOpEmitter opr, bool reverse)
{
	auto opDst = this->Operand(Dst());
	auto opSrc1 = this->Operand(Src1());
	auto opSrc2 = this->Operand(Src2());
	if (reverse)
	{
		auto tmp = opSrc1;
		opSrc1 = opSrc2;
		opSrc2 = tmp;
	}
	// We do not take the trouble of widening the CF to the word size
	// to simplify code analysis in later stages. 
	auto c = host->EnsureFlagGroup(ARM_REG_CPSR, (int)FlagM::CF, "C", BaseType::Bool);
	m.Assign(
		opDst,
		(m.*opr)(
			(m.*opr)(opSrc1, opSrc2),
			c));
	MaybeUpdateFlags(opDst);
}

class Bits
{
public:
	static uint32_t Mask32(int lsb, int bitsize)
	{
		return ((1u << bitsize) - 1) << lsb;
	}
};

void ArmRewriter::RewriteBfc()
{
	auto opDst = this->Operand(Dst());
	auto lsb = instr->detail->arm.operands[1].imm;
	auto bitsize = instr->detail->arm.operands[2].imm;
	m.Assign(opDst, m.And(opDst, m.UInt32(~Bits::Mask32(lsb, bitsize))));
}

void ArmRewriter::RewriteBfi()
{
	auto opDst = this->Operand(Dst());
	auto opSrc = this->Operand(Src1());
	auto tmp = host->CreateTemporary(BaseType::Word32);
	auto lsb = instr->detail->arm.operands[2].imm;
	auto bitsize = instr->detail->arm.operands[3].imm;
	m.Assign(tmp, m.Slice(opSrc, 0, bitsize));
	m.Assign(opDst, m.Dpb(opDst, tmp, lsb));
}

void ArmRewriter::RewriteBinOp(BinOpEmitter op, bool setflags)
{
	auto opDst = this->Operand(Dst());
	auto opSrc1 = this->Operand(Src1());
	auto opSrc2 = this->Operand(Src2());
	m.Assign(opDst, (m.*op)(opSrc1, opSrc2));
	if (setflags)
	{
		m.Assign(NZCV(), m.Cond(opDst));
	}
}

void ArmRewriter::RewriteRev()
{
	auto opDst = this->Operand(Dst());
	auto ppp = host->EnsurePseudoProcedure("__rev", BaseType::Word32, 1);
	m.AddArg(this->Operand(Src1()));
	m.Assign(opDst, m.Fn(ppp));
}

void ArmRewriter::RewriteRevBinOp(BinOpEmitter op, bool setflags)
{
	auto opDst = this->Operand(Dst());
	auto opSrc1 = this->Operand(Src1());
	auto opSrc2 = this->Operand(Src2());
	m.Assign(opDst, (m.*op)(opSrc1, opSrc2));
	if (setflags)
	{
		m.Assign(NZCV(), m.Cond(opDst));
	}
}

void ArmRewriter::RewriteUnaryOp(UnaryOpEmitter op)
{
	auto opDst = this->Operand(Dst());
	auto opSrc = this->Operand(Src1());
	m.Assign(opDst, (m.*op)(opSrc));
	if (instr->detail->arm.update_flags)
	{
		m.Assign(NZCV(), m.Cond(opDst));
	}
}

void ArmRewriter::RewriteBic()
{
	auto opDst = this->Operand(Dst());
	auto opSrc1 = this->Operand(Src1());
	auto opSrc2 = this->Operand(Src2());
	m.Assign(opDst, m.And(opSrc1, m.Comp(opSrc2)));
}

void ArmRewriter::RewriteClz()
{
	auto opDst = this->Operand(Dst());
	auto opSrc = this->Operand(Src1());
	auto ppp = host->EnsurePseudoProcedure("__clz", BaseType::Int32, 1);
	m.AddArg(opSrc);
	m.Assign(opDst, m.Fn(ppp));
}

void ArmRewriter::RewriteCmn()
{
	auto opDst = this->Operand(Dst());
	auto opSrc = this->Operand(Src1());
	m.Assign(
		NZCV(),
		m.Cond(
			m.IAdd(opDst, opSrc)));
}

void ArmRewriter::RewriteCmp()
{
	auto opDst = this->Operand(Dst());
	auto opSrc = this->Operand(Src1());
	m.Assign(
		NZCV(),
		m.Cond(
			m.ISub(opDst, opSrc)));
}

void ArmRewriter::RewriteTeq()
{
	auto opDst = this->Operand(Dst());
	auto opSrc = this->Operand(Src1());
	m.Assign(
		NZCV(),
		m.Cond(m.Xor(opDst, opSrc)));
}

void ArmRewriter::RewriteTst()
{
	auto opDst = this->Operand(Dst());
	auto opSrc = this->Operand(Src1());
	m.Assign(
		NZCV(),
		m.Cond(m.And(opDst, opSrc)));
}

void ArmRewriter::RewriteLdr(BaseType size)
{
	auto opSrc = this->Operand(Src1());
	auto opDst = this->Operand(Dst());
	auto rDst = Dst().reg;
	if (rDst == ARM_REG_PC) 
	{
		// Assignment to PC is the same as a jump
		m.Goto(opSrc);
		rtlClass = instr->detail->arm.cc == ARM_CC_AL
			? RtlClass::Transfer
			: RtlClass::ConditionalTransfer;
		return;
	}
	m.Assign(opDst, opSrc);
	MaybePostOperand(Src1());
}

void ArmRewriter::RewriteLdrd()
{
	auto ops = instr->detail->arm.operands;
	auto regLo = (int)ops[0].reg;
	auto regHi = (int)ops[1].reg;
	auto opDst = host->EnsureSequence(regHi, regLo, BaseType::Word64);
	auto opSrc = this->Operand(ops[2]);
	m.Assign(opDst, opSrc);
	MaybePostOperand(ops[2]);
}

void ArmRewriter::RewriteStr(BaseType size)
{
	auto opSrc = this->Operand(Dst());
	auto opDst = this->Operand(Src1());
	if (size != BaseType::Word32)
	{
		opSrc = m.Cast(size, opSrc);
	}
	m.Assign(opDst, opSrc);
	MaybePostOperand(Src1());
}

void ArmRewriter::RewriteStrd()
{
	auto ops = instr->detail->arm.operands;
	auto regLo = (int)ops[0].reg;
	auto regHi = (int)ops[1].reg;
	auto opSrc = host->EnsureSequence(regHi, regLo, BaseType::Word64);
	auto opDst = this->Operand(ops[2]);
	m.Assign(opDst, opSrc);
	MaybePostOperand(ops[2]);
}

void ArmRewriter::RewriteMultiplyAccumulate(BinOpEmitter op)
{
	auto opDst = this->Operand(Dst());
	auto opSrc1 = this->Operand(Src1());
	auto opSrc2 = this->Operand(Src2());
	auto opSrc3 = this->Operand(Src3());
	m.Assign(opDst, (m.*op)(opSrc3, m.IMul(opSrc1, opSrc2)));
	if (instr->detail->arm.update_flags)
	{
		m.Assign(NZCV(), m.Cond(opDst));
	}
}

void ArmRewriter::RewriteMov()
{
	if (Dst().type == ARM_OP_REG && Dst().reg == ARM_REG_PC)
	{
		rtlClass = RtlClass::Transfer;
		if (Src1().type == ARM_OP_REG && Src1().reg == ARM_REG_LR)
		{
			m.Return(0, 0);
		}
		else
		{
			m.Goto(Operand(Src1()));
		}
		m.FinishCluster(RtlClass::Transfer, address, instr->size);
		return;
	}
	auto opDst = Operand(Dst());
	auto opSrc = Operand(Src1());
	m.Assign(opDst, opSrc);
}

void ArmRewriter::RewriteMovt()
{
	auto opDst = Operand(Dst());
	auto iSrc = Src1().imm;
	auto opSrc = m.Dpb(opDst, m.Word16((uint16_t)iSrc), 16);
	m.Assign(opDst, opSrc);
}

void ArmRewriter::RewriteLdm(int initialOffset, BinOpEmitter op)
{
	auto dst = this->Operand(Dst());
	auto ops = &instr->detail->arm.operands[0];
	auto begin = ops + 1;
	auto end = ops + instr->detail->arm.op_count;
	RewriteLdm(dst, 1, initialOffset, op, instr->detail->arm.writeback);
}

void ArmRewriter::RewriteLdm(HExpr dst, int skip, int offset, BinOpEmitter op, bool writeback)
{
	bool pcRestored = false;
	auto begin = &instr->detail->arm.operands[skip];
	auto end = begin + (instr->detail->arm.op_count - skip);
	for (auto r = begin; r != end; ++r)
	{
		HExpr ea = offset != 0
			? (m.*op)(dst, m.Int32(offset))
			: dst;
		if (r->reg == ARM_REG_PC)
		{
			pcRestored = true;
		}
		else
		{
			auto dstReg = Reg(r->reg);
			m.Assign(dstReg, m.Mem32(ea));
		}
		offset += 4;
	}
	if (writeback)
	{
		m.Assign(dst, m.IAdd(dst, m.Int32(offset)));
	}
	if (pcRestored)
	{
		rtlClass = instr->detail->arm.cc == ARM_CC_AL
			? RtlClass::Transfer
			: RtlClass::ConditionalTransfer;
		m.Return(0, 0);
	}
}

void ArmRewriter::RewriteMla(bool hiLeft, bool hiRight, BaseType dt, BinOpEmitter op)
{
	auto dst = Operand(Dst());

	auto left = Operand(Src1());
	left = hiLeft ? m.Sar(left, m.Int32(16)) : left;
	left = m.Cast(dt, left);

	auto right = Operand(Src2());
	right = hiRight ? m.Sar(right, m.Int32(16)) : right;
	right = m.Cast(dt, right);

	m.Assign(dst, m.IAdd((m.*op)(left, right), Operand(Src3())));
	m.Assign(Q(), m.Cond(dst));
}

void ArmRewriter::RewriteMulbb(bool hiLeft, bool hiRight, BaseType dt, BinOpEmitter mul)
{
	auto dst = Operand(Dst());

	auto left = Operand(Src1());
	left = hiLeft ? m.Sar(left, m.Int32(16)) : left;
	left = m.Cast(dt, left);

	auto right = Operand(Src2());
	right = hiRight ? m.Sar(right, m.Int32(16)) : right;
	right = m.Cast(dt, right);

	m.Assign(dst, (m.*mul)(left, right));
}

void ArmRewriter::RewriteMull(BaseType dtResult, BinOpEmitter op)
{
	auto ops = instr->detail->arm.operands;
	auto regLo = (int)ops[0].reg;
	auto regHi = (int)ops[1].reg;

	auto opDst = host->EnsureSequence(regHi, regLo, dtResult);
	auto opSrc1 = this->Operand(Src3());
	auto opSrc2 = this->Operand(Src2());
	m.Assign(opDst, (m.*op)(opSrc1, opSrc2));
	if (instr->detail->arm.update_flags)
	{
		m.Assign(NZCV(), m.Cond(opDst));
	}
}

void ArmRewriter::RewritePop()
{
	auto sp = Reg(ARM_REG_SP);
	RewriteLdm(sp, 0, 0, &INativeRtlEmitter::IAdd, true);
}

void ArmRewriter::RewritePush()
{
	int offset = 0;
	auto dst = Reg(ARM_REG_SP);
	auto begin = &instr->detail->arm.operands[0];
	auto end = begin + instr->detail->arm.op_count;
	for (auto op = begin; op != end; ++op)
	{
		auto ea = offset != 0
			? m.ISub(dst, m.Int32(offset))
			: dst;
		auto reg = Reg(op->reg);
		m.Assign(m.Mem32(ea), reg);
		offset += 4;
	}
	m.Assign(dst, m.ISub(dst, m.Int32(offset)));
}

void ArmRewriter::RewriteQAddSub(BinOpEmitter op)
{
	auto dst = Operand(Dst());
	auto src1 = Operand(Src1());
	auto src2 = Operand(Src2());
	auto sum = (m.*op)(src1, src2);
	auto sat = host->EnsurePseudoProcedure("__signed_sat_32", BaseType::Int32, 1);
	m.AddArg(sum);
	m.Assign(dst, m.Fn(sat));
	m.Assign(
		Q(),
		m.Cond(dst));
}

void ArmRewriter::RewriteQDAddSub(BinOpEmitter op)
{
	auto sat = host->EnsurePseudoProcedure("__signed_sat_32", BaseType::Int32, 1);
	auto dst = Operand(Dst());
	auto src1 = m.SMul(Operand(Src1()), m.Int32(2));
	m.AddArg(src1);
	src1 = m.Fn(sat);
	auto src2 = Operand(Src2());
	auto sum = (m.*op)(src2, src1);
	m.AddArg(sum);
	m.Assign(dst, m.Fn(sat));
	m.Assign(
		host->EnsureFlagGroup((int)ARM_REG_CPSR, 0x10, "Q", BaseType::Bool),
		m.Cond(dst));
}


void ArmRewriter::RewriteSbfx()
{
	auto dst = this->Operand(Dst());
	auto src = m.Cast(
		BaseType::Int32,
		m.Slice(
			this->Operand(Src1()),
			Src2().imm,
			Src3().imm));
	m.Assign(dst, src);
}


void ArmRewriter::RewriteSmlal()
{
	auto dst = host->EnsureSequence(Dst().reg, Src1().reg, BaseType::Int64);
	auto fac1 = Operand(Src2());
	auto fac2 = Operand(Src3());
	m.Assign(dst, m.IAdd(m.SMul(fac1, fac2), dst));
}

void ArmRewriter::RewriteMlal(bool hiLeft, bool hiRight, BaseType dt, BinOpEmitter op)
{
	auto dst = host->EnsureSequence(Dst().reg, Src1().reg, BaseType::Int64);

	auto left = Operand(Src2());
	left = hiLeft ? m.Sar(left, m.Int32(16)) : left;
	left = m.Cast(dt, left);

	auto right = Operand(Src3());
	right = hiRight ? m.Sar(right, m.Int32(16)) : right;
	right = m.Cast(dt, right);

	m.Assign(dst, m.IAdd((m.*op)(left, right), dst));
}


void ArmRewriter::RewriteSmlaw(bool highPart)
{
	auto dst = this->Operand(Dst());
	auto fac1 = this->Operand(Src1());
	auto fac2 = this->Operand(Src2());
	fac2 = m.Cast(BaseType::Int16, highPart ? m.Sar(fac2, m.Int32(16)) : fac2);
	
	auto acc = this->Operand(Src3());
	m.Assign(dst, m.IAdd(
		m.Sar(
			m.SMul(fac1, fac2),
			m.Int32(16)),
		acc));
}

void ArmRewriter::RewriteMulw(bool highPart)
{
	auto dst = this->Operand(Dst());
	auto fac1 = this->Operand(Src1());
	auto fac2 = this->Operand(Src2());
	fac2 = m.Cast(BaseType::Int16, highPart ? m.Sar(fac2, m.Int32(16)) : fac2);
	m.Assign(dst, m.Sar(
		m.SMul(fac1, fac2),
		m.Int32(16)));
}


void ArmRewriter::RewriteStm(int offset, bool inc)
{
	auto dst = this->Operand(Dst());
	auto begin = &instr->detail->arm.operands[1];	// Skip the dst register
	auto end = begin + instr->detail->arm.op_count - 1;
	auto increment = inc ? 4 : -4;
	for (auto r = begin; r != end; ++r)
	{
		auto ea = offset > 0
			? m.IAdd(dst, m.Int32(offset))
			: offset < 0
			? m.ISub(dst, m.Int32(abs(offset)))
			: dst;
		auto srcReg = Reg(r->reg);
		m.Assign(m.Mem32(ea), srcReg);
		offset += increment;
	}
	if (instr->detail->arm.writeback)
	{
		if (offset > 0)
		{
			m.Assign(dst, m.IAdd(dst, m.Int32(offset)));
		}
		else if (offset < 0)
		{
			m.Assign(dst, m.ISub(dst, m.Int32(abs(offset))));
		}
	}
}

void ArmRewriter::RewriteStmib()
{
	auto dst = this->Operand(Dst());
	auto begin = &instr->detail->arm.operands[1];	// Skip the dst register
	auto end = begin + instr->detail->arm.op_count - 1;
	int offset = 4;
	for (auto r = begin; r != end; ++r)
	{
		auto ea = m.IAdd(dst, m.Int32(offset));
		auto srcReg = Reg(r->reg);
		m.Assign(m.Mem32(ea), srcReg);
		offset += 4;
	}
	if (offset != 4 && instr->detail->arm.writeback)
	{
		m.Assign(dst, m.IAdd(dst, m.Int32(offset)));
	}
}

void ArmRewriter::RewriteUbfx()
{
	auto dst = this->Operand(Dst());
	auto src = m.Cast(
		BaseType::UInt32,
		m.Slice(
			this->Operand(Src1()),
			Src2().imm,
			Src3().imm));
	m.Assign(dst, src);
}

void ArmRewriter::RewriteUmaal()
{
	auto tmp = host->CreateTemporary(BaseType::UInt64);
	auto lo = Operand(Dst());
	auto hi = Operand(Src1());
	auto rn = Operand(Src2());
	auto rm = Operand(Src3());
	auto dst = host->EnsureSequence(
		(int)Src1().reg,
		(int)Dst().reg,
		BaseType::UInt64);
	m.Assign(tmp, m.UMul(rn, rm));
	m.Assign(tmp, m.IAdd(tmp, m.Cast(BaseType::UInt64, hi)));
	m.Assign(dst, m.IAdd(tmp, m.Cast(BaseType::UInt64, lo)));
}

void ArmRewriter::RewriteUmlal()
{
	auto dst = host->EnsureSequence(
		(int)Src1().reg,
		(int)Dst().reg,
		BaseType::Word64);
	auto left = this->Operand(Src2());
	auto right = this->Operand(Src3());
	m.Assign(dst, m.IAdd(m.UMul(left, right), dst));
	MaybeUpdateFlags(dst);
}

void ArmRewriter::RewriteXtab(BaseType dt)
{
	auto dst = this->Operand(Dst());
	auto src = Reg((int)Src2().reg);
	if (Src2().shift.type == ARM_SFT_ROR)
	{
		src = m.Shr(src, m.Int32(Src2().shift.value));
	}
	src = m.Cast(dt, src);
	m.Assign(dst, m.IAdd(this->Operand(Src1()), src));
}

void ArmRewriter::RewriteXtb(BaseType dt)
{
	auto dst = this->Operand(Dst());
	auto src = Reg((int)Src1().reg);
	if (Src1().shift.type == ARM_SFT_ROR)
	{
		src = m.Shr(src, m.Int32(Src1().shift.value));
	}
	src = m.Cast(dt, src);
	m.Assign(dst, src);
}
