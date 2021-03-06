#pragma once
#include "target.h"


namespace ny {


inline nytarget_t* CTarget::self() {
	return reinterpret_cast<nytarget_t*>(this);
}


inline const nytarget_t* CTarget::self() const {
	return reinterpret_cast<const nytarget_t*>(this);
}


inline AnyString CTarget::name() const {
	return m_name;
}


template<class T>
inline void CTarget::eachSource(const T& callback) {
	for (auto& ptr : m_sources)
		callback(*ptr);
}


} // namespace nany
