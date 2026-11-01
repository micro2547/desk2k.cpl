#pragma once
#include "MonitorLayout.h"

// Pure geometry shared with the arrangement view; no display API calls or allocations.
namespace MonitorArrangementGeometry
{
	struct Projection
	{
		RECT desktop = {};
		RECT canvas = {};
		double scale = 1.0;
		double offsetX = 0.0;
		double offsetY = 0.0;
	};

	Projection Fit(const MonitorLayoutItem* items, size_t count, RECT client, UINT dpi);
	RECT Project(const Projection& projection, RECT desktop);
	POINT UnprojectDelta(const Projection& projection, POINT delta);
	// Finds the nearest nonoverlapping edge-connected placement among attached displays.
	POINT SnapPosition(const MonitorLayoutItem* items, size_t count, size_t index,
		POINT desired, LONG alignmentTolerance);
}

class CMonitorArrangement final
{
public:
	CMonitorArrangement() = default;
	~CMonitorArrangement();
	CMonitorArrangement(const CMonitorArrangement&) = delete;
	CMonitorArrangement& operator=(const CMonitorArrangement&) = delete;

	bool Attach(HWND window);
	void Detach();
	// The owner keeps the array alive until the next SetMonitors or Detach call.
	void SetMonitors(const MonitorLayoutItem* items, size_t count, size_t selected);

private:
	struct CachedMonitor
	{
		RECT box = {};
		HFONT font = nullptr;
		WCHAR label[16] = {};
		int labelLength = 0;
	};

	static LRESULT CALLBACK SubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
	LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void RebuildProjection();
	void ClearCache();
	void Paint(HDC dc);
	void PaintMonitor(HDC dc, size_t index, RECT box);
	size_t HitTest(POINT point) const;
	void Select(size_t index);
	void Notify(MonitorLayoutAction action, size_t index, POINT position = {}, DWORD orientation = DMDO_DEFAULT);
	void CancelDrag();
	void UpdateDrag(POINT point);
	void FinishDrag();
	void ShowContextMenu(POINT screenPoint);
	void HandleKey(UINT key);

	HWND m_window = nullptr;
	const MonitorLayoutItem* m_items = nullptr;
	size_t m_count = 0;
	size_t m_selected = 0;
	std::vector<CachedMonitor> m_cache;
	MonitorArrangementGeometry::Projection m_projection;
	UINT m_dpi = 96;
	HBRUSH m_canvasBrush = nullptr;
	HBRUSH m_monitorBrush = nullptr;
	HBRUSH m_selectionBrush = nullptr;
	bool m_pressed = false;
	bool m_dragging = false;
	size_t m_dragIndex = 0;
	POINT m_dragStart = {};
	POINT m_dragPosition = {};
	RECT m_dragOriginal = {};
	RECT m_dragBox = {};
};
