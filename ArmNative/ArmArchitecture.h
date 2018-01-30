#pragma once
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

class ArmArchitecture : public ComBase, public INativeArchitecture
{
public:
	ArmArchitecture();

	virtual STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObject) override;
	virtual STDMETHODIMP_(ULONG) AddRef(void) override { return ComBase::AddRef(); }
	virtual STDMETHODIMP_(ULONG) Release(void) override{ return ComBase::Release(); }

    STDMETHODIMP_(void) GetAllRegisters(int category, int * pcRegs, const NativeRegister * * ppRegs) override;
	STDMETHODIMP_(INativeDisassembler *) CreateDisassembler(const uint8_t * bytes, int length, int offset, uint64_t uAddr) override;
	STDMETHODIMP_(INativeRewriter *) CreateRewriter(
		const uint8_t * rawBytes,
		uint32_t length,
		uint32_t offset,
		uint64_t address,
		INativeRtlEmitter * m,
		INativeTypeFactory * typeFactory,
		INativeRewriterHost * host) override;
public:
	static const NativeRegister aRegs[110];
	static const NativeRegister aSysregs[19];
};