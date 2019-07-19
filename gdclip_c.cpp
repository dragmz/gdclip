#include <gdnative_api_struct.gen.h>
#include <stdio.h>
#include <string.h>
#include "clipper/clipper.hpp"
#include "gdclip.h"

const godot_gdnative_core_api_struct *api = NULL;
const godot_gdnative_ext_nativescript_api_struct *nativescript_api = NULL;

extern "C" {
	void *gdclip_constructor(godot_object *p_instance, void *p_method_data);
	void gdclip_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data);
	godot_variant gdclip_diff(godot_object *p_instance, void *p_method_data, void *p_user_data, int p_num_args, godot_variant **p_args);

	typedef struct user_data_struct {
		char data[256];
	} user_data_struct;

	void GDN_EXPORT godot_gdnative_init(godot_gdnative_init_options *p_options) {
		api = p_options->api_struct;

		for (unsigned int i = 0; i < api->num_extensions; i++)
		{
			switch (api->extensions[i]->type) {
			case GDNATIVE_EXT_NATIVESCRIPT:
				nativescript_api = (godot_gdnative_ext_nativescript_api_struct *)api->extensions[i];
				break;
			default:
				break;
			}
		}
	}

	void GDN_EXPORT godot_gdnative_terminate(godot_gdnative_terminate_options *p_options) {
		api = NULL;
		nativescript_api = NULL;
	}

	void GDN_EXPORT godot_nativescript_init(void *p_handle) {
		godot_instance_create_func create = { NULL, NULL, NULL };
		create.create_func = &gdclip_constructor;

		godot_instance_destroy_func destroy = { NULL, NULL, NULL };
		destroy.destroy_func = &gdclip_destructor;

		nativescript_api->godot_nativescript_register_class(p_handle, "GDCLIP", "Reference",
			create, destroy);

		godot_instance_method diff = { NULL, NULL, NULL };
		diff.method = &gdclip_diff;

		godot_method_attributes attributes = { GODOT_METHOD_RPC_MODE_DISABLED };

		nativescript_api->godot_nativescript_register_method(p_handle, "GDCLIP", "diff",
			attributes, diff);
	}

	void *gdclip_constructor(godot_object *p_instance, void *p_method_data) {
		user_data_struct *user_data = (user_data_struct*)api->godot_alloc(sizeof(user_data_struct));
		strcpy_s(user_data->data, 256, "World from GDNative!");

		return user_data;
	}

	void gdclip_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data) {
		api->godot_free(p_user_data);
	}

	godot_variant gdclip_diff(godot_object *p_instance, void *p_method_data,
		void *p_user_data, int p_num_args, godot_variant **p_args) {

		godot_pool_vector2_array subject = api->godot_variant_as_pool_vector2_array(p_args[0]);
		godot_pool_vector2_array clip = api->godot_variant_as_pool_vector2_array(p_args[1]);

		auto result = Difference(subject, clip);

		api->godot_pool_vector2_array_destroy(&subject);
		api->godot_pool_vector2_array_destroy(&clip);

		godot_variant ret;
		api->godot_variant_new_array(&ret, &result);
		api->godot_array_destroy(&result);

		return ret;
	}
}