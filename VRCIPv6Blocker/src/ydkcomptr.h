#pragma once

// ydkcomptr.h

namespace ydk {
	template<class T>
	class ComPtr {
		T* p_ = nullptr;
	public:
		ComPtr() = default;
		explicit ComPtr(T* p) : p_(p) {}
		ComPtr(const ComPtr&) = delete;
		ComPtr& operator=(const ComPtr&) = delete;
		ComPtr(ComPtr&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
		ComPtr& operator=(ComPtr&& o) noexcept {
			if (this != &o) { reset(); p_ = o.p_; o.p_ = nullptr; }
			return *this;
		}
		~ComPtr() { reset(); }

		T* get()  const noexcept { return p_; }
		T** put()        noexcept { reset(); return &p_; } // 受け渡し用
		T* detach()     noexcept { T* t = p_; p_ = nullptr; return t; }
		void reset(T* p = nullptr) noexcept { if (p_) p_->Release(); p_ = p; }
		T* operator->() const noexcept { return p_; }
		explicit operator bool() const noexcept { return p_ != nullptr; }
	};
}
