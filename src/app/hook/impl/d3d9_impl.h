#ifndef __D3D9_IMPL_H__
#define __D3D9_IMPL_H__

#include "shared.h"

namespace impl
{
	namespace d3d9
	{
		bool init();
		bool has_call_site_callback();
		bool has_factory_callback();
		bool has_create_device_callback();
		void cleanup();
	}
}

#endif // __D3D9_IMPL_H__
