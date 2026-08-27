#ifndef __D3D9_IMPL_H__
#define __D3D9_IMPL_H__

#include "shared.h"

namespace impl
{
	namespace d3d9
	{
		bool init();
		bool has_direct3d_create9_callback();
		bool has_create_device_callback();
		void cleanup();
	}
}

#endif // __D3D9_IMPL_H__
