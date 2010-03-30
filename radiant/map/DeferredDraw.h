#ifndef DEFERREDDRAW_H_
#define DEFERREDDRAW_H_

#include "generic/callbackfwd.h"
#include "map/Map.h"

class DeferredDraw
{
	Callback m_draw;
	bool m_defer;
	bool m_deferred;
public:
	DeferredDraw(const Callback& draw) : 
		m_draw(draw), 
		m_defer(false), 
		m_deferred(false)
	{}
	
	void defer() {
		m_defer = true;
	}
	
	void draw() {
		if (m_defer) {
			m_deferred = true;
		}
		else {
			m_draw();
		}
	}
	
	void flush() {
		if (m_defer && m_deferred) {
			m_draw();
		}
		m_deferred = false;
		m_defer = false;
	}

	// Callback target
	void onMapValidChanged()
	{
		if (GlobalMap().isValid())
		{
			flush();
		}
		else
		{
			defer();
		}
	}
};

#endif /*DEFERREDDRAW_H_*/
