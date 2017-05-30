#include "netvid_hook.h"

#include <iostream>

#include "../netvid/net.h"

template<class sender_type>
struct sender_wrapper
{
	netvid::io_service_wrapper io_service;
	netvid::socket_wrapper socket={ io_service.io_service };
	netvid::sender<sender_type> sender={socket};

	void operator()(const void *frame, unsigned frame_width, unsigned frame_height, unsigned bpp, unsigned pitch, float aspect_ratio)
	{
		if (frame_width==0 || frame_height==0 || pitch==0)
			return;

		frame_data in_frame;

		in_frame.data=const_cast<std::uint8_t *>(static_cast<const std::uint8_t *>(frame));
		in_frame.width=frame_width;
		in_frame.height=frame_height;
		in_frame.pitch=pitch;
		in_frame.bpp=bpp;
		in_frame.aspect_ratio=aspect_ratio;

		if (!in_frame.data)
			return;

		static std::tuple<float, int, int, int> last_settings;
		std::tuple<float, int, int, int> settings={ aspect_ratio, frame_width, frame_height, bpp };

		if (settings!=last_settings)
		{
			std::cout << "New framebuffer configuration: AR: " << aspect_ratio << " Resolution: " << frame_width << "x" << frame_height << " " << bpp << "bpp" << std::endl;

			last_settings=settings;
		}

		static bool failed=false;

		try
		{
			static std::promise<void> pr;
			static std::future<void> fu;
			static frame_data_managed transmit_frame;

			if (fu.valid())
			{
				if (fu.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
					return;
			}

			pr={};
			fu=pr.get_future();
			transmit_frame.copy(in_frame);
			io_service.io_service.post([&] { sender.send(transmit_frame, pr); });

			failed=false;
		}
		catch (const std::exception &e)
		{
			if (failed)
				return;

			failed=true;

			std::cerr << "Failed to send frame (" << in_frame.width << "x" << in_frame.height << "x" << in_frame.bpp << "bpp, " << in_frame.bytes() << " bytes): " << e.what() << std::endl;
		}
	}
};

boost::optional<bool> to_bool(const char *sz)
{
	if (!sz)
		return boost::none;

	std::string s(sz);

	return s!="false" && s!="0";
}

typedef std::function<void(const void *frame, unsigned frame_width, unsigned frame_height, unsigned bpp, unsigned pitch, float aspect_ratio)> new_frame_func_t;

template<class sender_type>
struct netvid_init_tryer
{
	const char *remote_endpoint_env="remote_gfx_endpoint";
	const char *rate_limit_env="remote_gfx_rate_limit";

	const char *remote_endpoint=getenv(remote_endpoint_env);
	const char *rate_limit_s=getenv(rate_limit_env);

	netvid_init_tryer()
	{

	}

	bool operator()(new_frame_func_t &new_frame_func);

	std::unique_ptr<sender_wrapper<sender_type>> make()
	{
		return std::make_unique<sender_wrapper<sender_type>>();
	}

	bool wrap(new_frame_func_t &new_frame_func, std::unique_ptr<sender_wrapper<sender_type>> &&sender)
	{
		auto shared=std::shared_ptr<sender_wrapper<sender_type>>(sender.release());
		auto func=[shared](const void *frame, unsigned frame_width, unsigned frame_height, unsigned bpp, unsigned pitch, float aspect_ratio)
		{
			(*shared)(frame, frame_width, frame_height, bpp, pitch, aspect_ratio);
		};

		new_frame_func=std::move(func);
		shared->io_service.run();

		return true;
	}
};

template<>
bool netvid_init_tryer<netvid::rate_limited_sender>::operator()(new_frame_func_t &new_frame_func)
{
	if (!remote_endpoint)
		return false;

	if (!rate_limit_s)
		return false;

	auto sender=make();

	sender->sender.set_remote_endpoint(remote_endpoint);
	sender->sender.max_rate_bytes=std::atoi(rate_limit_s)/8;

	return wrap(new_frame_func, std::move(sender));
};

template<>
bool netvid_init_tryer<netvid::unlimited_sender>::operator()(new_frame_func_t &new_frame_func)
{
	if (!remote_endpoint)
		return false;

	auto sender=make();

	sender->sender.set_remote_endpoint(remote_endpoint);

	return wrap(new_frame_func, std::move(sender));
};

template<class sender_type>
bool netvid_try_init(new_frame_func_t &new_frame_func)
{
	netvid_init_tryer<sender_type> nit;

	return nit(new_frame_func);
}

new_frame_func_t netvid_init()
{
	new_frame_func_t ret;

	false ||
		netvid_try_init<netvid::rate_limited_sender>(ret) ||
		netvid_try_init<netvid::unlimited_sender>(ret);

	return ret;
}

void netvid_new_frame(const void *frame, unsigned frame_width, unsigned frame_height, unsigned bpp, unsigned pitch, float aspect_ratio)
{
	static new_frame_func_t new_frame_func=netvid_init();

	if (new_frame_func)
		new_frame_func(frame, frame_width, frame_height, bpp, pitch, aspect_ratio);
}

void netvid_new_frame_bgr(const void *frame, unsigned frame_width, unsigned frame_height, unsigned bpp, unsigned pitch, float aspect_ratio)
{
	static new_frame_func_t new_frame_func=netvid_init();

	if (new_frame_func)
	{
		static frame_data_managed tmp_frame;
		frame_data in_frame;

		tmp_frame.resize(frame_width, frame_height, bpp);

		in_frame.data=static_cast<std::uint8_t *>(const_cast<void *>(frame));
		in_frame.width=frame_width;
		in_frame.height=frame_height;
		in_frame.bpp=bpp;
		in_frame.pitch=pitch;
		in_frame.aspect_ratio=aspect_ratio;

		for (int y=0; y<frame_height; ++y)
		{
			for (int x=0; x<frame_width; ++x)
			{
				std::uint8_t *c=in_frame.pixel<std::uint8_t>(x, y);

				std::swap(c[0], c[2]);

				*tmp_frame.pixel<std::uint32_t>(x, y)=*reinterpret_cast<std::uint32_t *>(c);
			}
		}

		new_frame_func(tmp_frame.data, frame_width, frame_height, bpp, tmp_frame.pitch, aspect_ratio);
	}
}

