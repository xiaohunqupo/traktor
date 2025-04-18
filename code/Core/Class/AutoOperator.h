/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Class/AutoVerify.h"
#include "Core/Class/CastAny.h"
#include "Core/Class/IRuntimeDispatch.h"
#include "Core/Meta/MethodSignature.h"

namespace traktor
{

/*! \ingroup Core */
/*! \{ */

/*! \name Operator */
/*! \{ */

template <
	typename ClassType,
	typename ReturnType,
	typename Argument1Type
>
struct Operator final : public IRuntimeDispatch
{
	T_NO_COPY_CLASS(Operator);

	typedef typename MethodSignature< true, ClassType, ReturnType, Argument1Type >::method_t method_t;

	method_t m_method;

	explicit Operator(method_t method)
	:	m_method(method)
	{
	}

#if defined(T_NEED_RUNTIME_SIGNATURE)
	virtual void signature(OutputStream& os) const override final {}
#endif

	virtual Any invoke(ITypedObject* self, uint32_t argc, const Any* argv) const override final
	{
		T_VERIFY_ARGUMENT_COUNT(1)

		if (CastAny< Argument1Type >::accept(argv[0])) [[likely]]
		{
			return CastAny< ReturnType >::set((T_VERIFY_CAST_SELF(ClassType, self)->*m_method)(
				CastAny< Argument1Type >::get(argv[0])
			));
		}
		else [[unlikely]]
			return Any();
	}
};

/*! \} */

/*! \} */

}
