#include "pch.h"
#include "MonitorArrangement.h"
#include <cmath>
#include <limits>

namespace
{
	constexpr size_t NoMonitor = static_cast<size_t>(-1);
	constexpr UINT_PTR ArrangementSubclass = 1;
	constexpr UINT MenuLandscape = 1;
	constexpr UINT MenuPortrait = 2;
	constexpr UINT MenuLandscapeFlipped = 3;
	constexpr UINT MenuPortraitFlipped = 4;
	constexpr UINT MenuPrimary = 5;
	constexpr UINT MenuAttached = 6;

	LONG RoundedLong(double value)
	{
		const double low = static_cast<double>((std::numeric_limits<LONG>::min)());
		const double high = static_cast<double>((std::numeric_limits<LONG>::max)());
		return static_cast<LONG>((std::max)(low, (std::min)(high, std::round(value))));
	}

	long long ClampCoordinate(long long value, long long low, long long high)
	{
		return (std::max)(low, (std::min)(value, high));
	}

	bool HasArea(const RECT& rect)
	{
		return rect.right > rect.left && rect.bottom > rect.top;
	}
}

namespace MonitorArrangementGeometry
{
	Projection Fit(const MonitorLayoutItem* items, size_t count, RECT client, UINT dpi)
	{
		Projection result;
		// Like the original DeskScale/DeskOff transform, use one scale for both axes.
		const LONG width = (std::max)(0L, client.right - client.left);
		const LONG height = (std::max)(0L, client.bottom - client.top);
		const LONG border = (std::max)(static_cast<LONG>(MulDiv(12, dpi ? dpi : 96, 96)), height / 5);
		const LONG insetX = (std::min)(border, (std::max)(0L, (width - 1) / 2));
		const LONG insetY = (std::min)(border, (std::max)(0L, (height - 1) / 2));
		result.canvas = {client.left + insetX, client.top + insetY,
			client.right - insetX, client.bottom - insetY};
		bool found = false;
		for (size_t i = 0; items && i < count; ++i)
		{
			if (!HasArea(items[i].desktop)) continue;
			if (!found) result.desktop = items[i].desktop;
			else UnionRect(&result.desktop, &result.desktop, &items[i].desktop);
			found = true;
		}
		if (!found) return result;
		const double desktopWidth = static_cast<double>(result.desktop.right) - result.desktop.left;
		const double desktopHeight = static_cast<double>(result.desktop.bottom) - result.desktop.top;
		result.scale = (std::min)((std::max)(1L, result.canvas.right - result.canvas.left) / desktopWidth,
			(std::max)(1L, result.canvas.bottom - result.canvas.top) / desktopHeight);
		result.offsetX = result.canvas.left + ((result.canvas.right - result.canvas.left) - desktopWidth * result.scale) / 2.0;
		result.offsetY = result.canvas.top + ((result.canvas.bottom - result.canvas.top) - desktopHeight * result.scale) / 2.0;
		return result;
	}

	RECT Project(const Projection& projection, RECT desktop)
	{
		return {
			RoundedLong(projection.offsetX + (static_cast<double>(desktop.left) - projection.desktop.left) * projection.scale),
			RoundedLong(projection.offsetY + (static_cast<double>(desktop.top) - projection.desktop.top) * projection.scale),
			RoundedLong(projection.offsetX + (static_cast<double>(desktop.right) - projection.desktop.left) * projection.scale),
			RoundedLong(projection.offsetY + (static_cast<double>(desktop.bottom) - projection.desktop.top) * projection.scale)};
	}

	POINT UnprojectDelta(const Projection& projection, POINT delta)
	{
		if (projection.scale <= 0.0) return {};
		return {RoundedLong(delta.x / projection.scale), RoundedLong(delta.y / projection.scale)};
	}

	POINT SnapPosition(const MonitorLayoutItem* items, size_t count, size_t index,
		POINT desired, LONG alignmentTolerance)
	{
		if (!items || index >= count || !items[index].attached || !HasArea(items[index].desktop)) return desired;
		const RECT original = items[index].desktop;
		const long long width = static_cast<long long>(original.right) - original.left;
		const long long height = static_cast<long long>(original.bottom) - original.top;
		const long long coordinateMin = (std::numeric_limits<LONG>::min)();
		const long long coordinateMax = (std::numeric_limits<LONG>::max)();
		POINT best = {original.left, original.top};
		double bestDistance = (std::numeric_limits<double>::max)();
		const auto consider = [&](long long x, long long y)
		{
			if (x < coordinateMin || y < coordinateMin || x + width > coordinateMax || y + height > coordinateMax) return;
			for (size_t j = 0; j < count; ++j)
			{
				if (j == index || !items[j].attached || !HasArea(items[j].desktop)) continue;
				const RECT& other = items[j].desktop;
				if (x < other.right && x + width > other.left && y < other.bottom && y + height > other.top) return;
			}
			const double dx = static_cast<double>(x) - desired.x;
			const double dy = static_cast<double>(y) - desired.y;
			const double distance = dx * dx + dy * dy;
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = {static_cast<LONG>(x), static_cast<LONG>(y)};
			}
		};
		bool hasNeighbor = false;
		for (size_t i = 0; i < count; ++i)
		{
			if (i == index || !items[i].attached || !HasArea(items[i].desktop)) continue;
			hasNeighbor = true;
			const RECT& other = items[i].desktop;
			// Candidate placements touch an edge, not just a corner. Resolve overlaps
			// and close gaps without ever moving a different staged monitor.
			for (int side = 0; side < 4; ++side)
			{
				const bool verticalEdge = side < 2;
				const long long fixed = side == 0 ? static_cast<long long>(other.left) - width :
					side == 1 ? other.right : side == 2 ? static_cast<long long>(other.top) - height : other.bottom;
				const long long low = verticalEdge ? static_cast<long long>(other.top) - height + 1 :
					static_cast<long long>(other.left) - width + 1;
				const long long high = verticalEdge ? static_cast<long long>(other.bottom) - 1 :
					static_cast<long long>(other.right) - 1;
				long long along = ClampCoordinate(verticalEdge ? desired.y : desired.x, low, high);
				const long long alignStart = verticalEdge ? other.top : other.left;
				const long long alignEnd = verticalEdge ? static_cast<long long>(other.bottom) - height :
					static_cast<long long>(other.right) - width;
				if (std::abs(along - alignStart) <= alignmentTolerance) along = alignStart;
				else if (std::abs(along - alignEnd) <= alignmentTolerance) along = alignEnd;
				const auto onEdge = [&](long long coordinate)
				{
					if (coordinate < low || coordinate > high) return;
					if (verticalEdge) consider(fixed, coordinate);
					else consider(coordinate, fixed);
				};
				onEdge(along);
				onEdge(alignStart);
				onEdge(alignEnd);
				// The nearest clear interval can start at any intervening monitor edge.
				for (size_t j = 0; j < count; ++j)
				{
					if (j == index || !items[j].attached || !HasArea(items[j].desktop)) continue;
					onEdge(verticalEdge ? static_cast<long long>(items[j].desktop.top) - height :
						static_cast<long long>(items[j].desktop.left) - width);
					onEdge(verticalEdge ? items[j].desktop.bottom : items[j].desktop.right);
				}
			}
		}
		return hasNeighbor ? best : desired;
	}
}

CMonitorArrangement::~CMonitorArrangement()
{
	Detach();
}

bool CMonitorArrangement::Attach(HWND window)
{
	if (window == m_window && window) return true;
	Detach();
	if (!IsWindow(window)) return false;
	m_canvasBrush = CreateSolidBrush(RGB(128, 128, 128));
	m_monitorBrush = CreateSolidBrush(RGB(58, 110, 165));
	m_selectionBrush = CreateSolidBrush(RGB(0, 0, 128));
	if (!m_canvasBrush || !m_monitorBrush || !m_selectionBrush ||
		!SetWindowSubclass(window, SubclassProc, ArrangementSubclass, reinterpret_cast<DWORD_PTR>(this)))
	{
		Detach();
		return false;
	}
	m_window = window;
	m_dpi = GetDpiForWindow(window);
	if (!m_dpi) m_dpi = 96;
	RebuildProjection();
	return true;
}

void CMonitorArrangement::Detach()
{
	CancelDrag();
	if (m_window) RemoveWindowSubclass(m_window, SubclassProc, ArrangementSubclass);
	m_window = nullptr;
	m_items = nullptr;
	m_count = 0;
	ClearCache();
	if (m_canvasBrush) DeleteObject(m_canvasBrush);
	if (m_monitorBrush) DeleteObject(m_monitorBrush);
	if (m_selectionBrush) DeleteObject(m_selectionBrush);
	m_canvasBrush = m_monitorBrush = m_selectionBrush = nullptr;
}

void CMonitorArrangement::SetMonitors(const MonitorLayoutItem* items, size_t count, size_t selected)
{
	CancelDrag();
	m_items = items;
	m_count = items ? count : 0;
	m_selected = m_count ? (std::min)(selected, m_count - 1) : 0;
	RebuildProjection();
}

void CMonitorArrangement::ClearCache()
{
	for (auto& item : m_cache)
	{
		if (item.font) DeleteObject(item.font);
	}
	m_cache.clear();
}

void CMonitorArrangement::RebuildProjection()
{
	ClearCache();
	if (!m_window) return;
	RECT client;
	GetClientRect(m_window, &client);
	m_projection = MonitorArrangementGeometry::Fit(m_items, m_count, client, m_dpi);
	m_cache.resize(m_count);
	for (size_t i = 0; i < m_count; ++i)
	{
		auto& cached = m_cache[i];
		cached.box = MonitorArrangementGeometry::Project(m_projection, m_items[i].desktop);
		// Keep very small displays hittable, preserving their own aspect ratio.
		// Ordinary displays use the exact shared physical projection above.
		const double width = (std::max)(1.0, static_cast<double>(m_items[i].desktop.right) - m_items[i].desktop.left);
		const double height = (std::max)(1.0, static_cast<double>(m_items[i].desktop.bottom) - m_items[i].desktop.top);
		const LONG minimum = MulDiv(12, m_dpi, 96);
		if (cached.box.right - cached.box.left < minimum || cached.box.bottom - cached.box.top < minimum)
		{
			double scale = (std::max)(minimum / width, minimum / height);
			scale = (std::min)(scale, (std::min)((std::max)(1L, client.right - 8L) / width,
				(std::max)(1L, client.bottom - 8L) / height));
			const LONG visualWidth = (std::max)(1L, RoundedLong(width * scale));
			const LONG visualHeight = (std::max)(1L, RoundedLong(height * scale));
			const LONG left = RoundedLong((static_cast<double>(cached.box.left) + cached.box.right - visualWidth) / 2.0);
			const LONG top = RoundedLong((static_cast<double>(cached.box.top) + cached.box.bottom - visualHeight) / 2.0);
			cached.box = {left, top, left + visualWidth, top + visualHeight};
		}
		StringCchPrintfW(cached.label, ARRAYSIZE(cached.label), L"%d", m_items[i].number);
		cached.labelLength = lstrlenW(cached.label);
		const LONG boxHeight = cached.box.bottom - cached.box.top;
		const LONG boxWidth = cached.box.right - cached.box.left;
		const int fontHeight = (std::max)(1L, (std::min)(boxHeight * 3 / 4,
			boxWidth * 3 / (2 * (std::max)(1, cached.labelLength))));
		cached.font = CreateFontW(-fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			DEFAULT_PITCH | FF_SWISS, L"Arial");
	}
	InvalidateRect(m_window, nullptr, FALSE);
}

void CMonitorArrangement::PaintMonitor(HDC dc, size_t index, RECT box)
{
	const bool selected = index == m_selected;
	const bool enabled = IsWindowEnabled(m_window) != FALSE;
	const bool attached = m_items[index].attached && enabled;
	const int stroke = (std::max)(1, MulDiv(1, m_dpi, 96));
	if (selected)
	{
		RECT outer = box;
		InflateRect(&outer, stroke * 4, stroke * 4);
		FillRect(dc, &outer, enabled ? m_selectionBrush : GetSysColorBrush(COLOR_GRAYTEXT));
		InflateRect(&outer, -stroke * 3, -stroke * 3);
		FillRect(dc, &outer, GetSysColorBrush(COLOR_WINDOW));
	}
	else FrameRect(dc, &box, GetSysColorBrush(COLOR_WINDOW));
	RECT inner = box;
	InflateRect(&inner, -stroke, -stroke);
	if (HasArea(inner)) FillRect(dc, &inner, attached ? m_monitorBrush : GetSysColorBrush(COLOR_3DFACE));
	SetTextColor(dc, attached ? RGB(255, 255, 255) : GetSysColor(COLOR_GRAYTEXT));
	const auto& cached = m_cache[index];
	HGDIOBJ oldFont = SelectObject(dc, cached.font ? cached.font : GetStockObject(DEFAULT_GUI_FONT));
	DrawTextW(dc, cached.label, cached.labelLength, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	SelectObject(dc, oldFont);
}

void CMonitorArrangement::Paint(HDC dc)
{
	RECT client;
	GetClientRect(m_window, &client);
	FillRect(dc, &client, m_canvasBrush);
	DrawEdge(dc, &client, EDGE_SUNKEN, BF_RECT);
	SaveDC(dc);
	InflateRect(&client, -2, -2);
	IntersectClipRect(dc, client.left, client.top, client.right, client.bottom);
	SetBkMode(dc, TRANSPARENT);
	for (size_t i = 0; i < m_cache.size(); ++i)
	{
		if (i != m_selected) PaintMonitor(dc, i, m_cache[i].box);
	}
	if (m_selected < m_cache.size())
		PaintMonitor(dc, m_selected, m_dragging && m_dragIndex == m_selected ? m_dragBox : m_cache[m_selected].box);
	if (GetFocus() == m_window)
	{
		RECT focus = client;
		InflateRect(&focus, -2, -2);
		DrawFocusRect(dc, &focus);
	}
	RestoreDC(dc, -1);
}

size_t CMonitorArrangement::HitTest(POINT point) const
{
	if (m_selected < m_cache.size() && PtInRect(&m_cache[m_selected].box, point)) return m_selected;
	for (size_t i = m_cache.size(); i > 0; --i)
		if (PtInRect(&m_cache[i - 1].box, point)) return i - 1;
	return NoMonitor;
}

void CMonitorArrangement::Notify(MonitorLayoutAction action, size_t index, POINT position, DWORD orientation)
{
	if (!m_window || index >= m_count) return;
	MonitorLayoutNotification notification;
	notification.header.hwndFrom = m_window;
	notification.header.idFrom = GetDlgCtrlID(m_window);
	notification.header.code = MLN_ACTION;
	notification.action = action;
	notification.index = index;
	notification.position = position;
	notification.orientation = orientation;
	SendMessageW(GetParent(m_window), WM_NOTIFY, notification.header.idFrom, reinterpret_cast<LPARAM>(&notification));
}

void CMonitorArrangement::Select(size_t index)
{
	if (index >= m_count) return;
	m_selected = index;
	InvalidateRect(m_window, nullptr, FALSE);
	// Notify even when already selected, so a context action always has an
	// unambiguous selected vector index before the parent receives the action.
	Notify(MonitorLayoutAction::Select, index);
}

void CMonitorArrangement::CancelDrag()
{
	const bool active = m_pressed;
	m_pressed = m_dragging = false;
	if (m_window && GetCapture() == m_window) ReleaseCapture();
	if (active && m_window) InvalidateRect(m_window, nullptr, FALSE);
}

void CMonitorArrangement::UpdateDrag(POINT point)
{
	if (!m_pressed || m_dragIndex >= m_count) return;
	const LONG dx = point.x - m_dragStart.x;
	const LONG dy = point.y - m_dragStart.y;
	if (!m_dragging && std::abs(dx) < GetSystemMetricsForDpi(SM_CXDRAG, m_dpi) &&
		std::abs(dy) < GetSystemMetricsForDpi(SM_CYDRAG, m_dpi)) return;
	m_dragging = true;
	const POINT delta = MonitorArrangementGeometry::UnprojectDelta(m_projection, {dx, dy});
	m_dragPosition = {RoundedLong(static_cast<double>(m_dragOriginal.left) + delta.x),
		RoundedLong(static_cast<double>(m_dragOriginal.top) + delta.y)};
	m_dragBox = m_cache[m_dragIndex].box;
	OffsetRect(&m_dragBox, dx, dy);
	InvalidateRect(m_window, nullptr, FALSE);
}

void CMonitorArrangement::FinishDrag()
{
	if (!m_pressed) return;
	const bool moved = m_dragging;
	const size_t index = m_dragIndex;
	const POINT desired = m_dragPosition;
	const RECT original = m_dragOriginal;
	CancelDrag();
	if (!moved || index >= m_count) return;
	const LONG tolerance = (std::max)(1L, RoundedLong(MulDiv(8, m_dpi, 96) / m_projection.scale));
	const POINT snapped = MonitorArrangementGeometry::SnapPosition(m_items, m_count, index, desired, tolerance);
	if (snapped.x != original.left || snapped.y != original.top)
		Notify(MonitorLayoutAction::Move, index, snapped);
}

void CMonitorArrangement::ShowContextMenu(POINT screenPoint)
{
	CancelDrag();
	if (!m_count) return;
	if (screenPoint.x == -1 && screenPoint.y == -1)
	{
		if (m_selected >= m_cache.size()) return;
		const RECT& box = m_cache[m_selected].box;
		screenPoint = {(box.left + box.right) / 2, (box.top + box.bottom) / 2};
		ClientToScreen(m_window, &screenPoint);
	}
	else
	{
		POINT point = screenPoint;
		ScreenToClient(m_window, &point);
		const size_t hit = HitTest(point);
		if (hit == NoMonitor) return;
		Select(hit);
	}
	if (!m_window || m_selected >= m_count) return;
	const size_t index = m_selected;
	HMENU menu = CreatePopupMenu();
	if (!menu) return;
	AppendMenuW(menu, MF_STRING, MenuLandscape, L"Landscape");
	AppendMenuW(menu, MF_STRING, MenuPortrait, L"Portrait");
	AppendMenuW(menu, MF_STRING, MenuLandscapeFlipped, L"Landscape (flipped)");
	AppendMenuW(menu, MF_STRING, MenuPortraitFlipped, L"Portrait (flipped)");
	const DWORD orientations[] = {DMDO_DEFAULT, DMDO_90, DMDO_180, DMDO_270};
	for (UINT i = 0; i < ARRAYSIZE(orientations); ++i)
	{
		if (m_items[index].orientation == orientations[i])
		{
			CheckMenuRadioItem(menu, MenuLandscape, MenuPortraitFlipped, MenuLandscape + i, MF_BYCOMMAND);
			break;
		}
	}
	AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(menu, MF_STRING | (m_items[index].primary ? MF_CHECKED : 0) |
		(m_items[index].primary || !m_items[index].attached ? MF_GRAYED : 0),
		MenuPrimary, L"Use this device as the primary monitor");
	AppendMenuW(menu, MF_STRING | (m_items[index].attached ? MF_CHECKED : 0) |
		(m_items[index].primary ? MF_GRAYED : 0),
		MenuAttached, L"Extend my Windows desktop onto this monitor");
	const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
		screenPoint.x, screenPoint.y, m_window, nullptr);
	DestroyMenu(menu);
	if (!m_window || index >= m_count) return;
	if (command >= MenuLandscape && command <= MenuPortraitFlipped)
	{
		Notify(MonitorLayoutAction::Rotate, index, {}, orientations[command - MenuLandscape]);
	}
	else if (command == MenuPrimary) Notify(MonitorLayoutAction::MakePrimary, index);
	else if (command == MenuAttached) Notify(MonitorLayoutAction::ToggleAttached, index);
}

void CMonitorArrangement::HandleKey(UINT key)
{
	if (key == VK_ESCAPE)
	{
		CancelDrag();
		return;
	}
	if (!m_count || m_selected >= m_count) return;
	if (key == VK_APPS || (key == VK_F10 && GetKeyState(VK_SHIFT) < 0))
	{
		ShowContextMenu({-1, -1});
		return;
	}
	if (key == VK_HOME || key == VK_END || key == VK_PRIOR || key == VK_NEXT)
	{
		const size_t index = key == VK_HOME ? 0 : key == VK_END ? m_count - 1 :
			key == VK_PRIOR ? (m_selected + m_count - 1) % m_count : (m_selected + 1) % m_count;
		Select(index);
		return;
	}
	if (key < VK_LEFT || key > VK_DOWN) return;
	if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_SHIFT) < 0)
	{
		CancelDrag();
		const LONG step = GetKeyState(VK_SHIFT) < 0 ? 32 : 1;
		const RECT& rect = m_items[m_selected].desktop;
		POINT desired = {rect.left, rect.top};
		if (key == VK_LEFT) desired.x = RoundedLong(static_cast<double>(desired.x) - step);
		if (key == VK_RIGHT) desired.x = RoundedLong(static_cast<double>(desired.x) + step);
		if (key == VK_UP) desired.y = RoundedLong(static_cast<double>(desired.y) - step);
		if (key == VK_DOWN) desired.y = RoundedLong(static_cast<double>(desired.y) + step);
		const POINT snapped = MonitorArrangementGeometry::SnapPosition(m_items, m_count, m_selected, desired, 0);
		if (snapped.x != rect.left || snapped.y != rect.top) Notify(MonitorLayoutAction::Move, m_selected, snapped);
		return;
	}
	// Unmodified arrows choose the closest monitor in the requested direction.
	const RECT& current = m_cache[m_selected].box;
	const double cx = (current.left + current.right) / 2.0;
	const double cy = (current.top + current.bottom) / 2.0;
	size_t nearest = NoMonitor;
	double best = (std::numeric_limits<double>::max)();
	for (size_t i = 0; i < m_cache.size(); ++i)
	{
		if (i == m_selected) continue;
		const RECT& box = m_cache[i].box;
		const double dx = (box.left + box.right) / 2.0 - cx;
		const double dy = (box.top + box.bottom) / 2.0 - cy;
		if ((key == VK_LEFT && dx >= 0) || (key == VK_RIGHT && dx <= 0) ||
			(key == VK_UP && dy >= 0) || (key == VK_DOWN && dy <= 0)) continue;
		const double score = key == VK_LEFT || key == VK_RIGHT ? dx * dx + 4 * dy * dy : 4 * dx * dx + dy * dy;
		if (score < best) { best = score; nearest = i; }
	}
	if (nearest != NoMonitor) Select(nearest);
}

LRESULT CALLBACK CMonitorArrangement::SubclassProc(HWND window, UINT message, WPARAM wParam,
	LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
	auto* self = reinterpret_cast<CMonitorArrangement*>(reference);
	if (message == WM_NCDESTROY)
	{
		self->Detach();
		return DefSubclassProc(window, message, wParam, lParam);
	}
	return self->HandleMessage(message, wParam, lParam);
}

LRESULT CMonitorArrangement::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(m_window, &paint);
		Paint(dc);
		EndPaint(m_window, &paint);
		return 0;
	}
	case WM_PRINTCLIENT:
		Paint(reinterpret_cast<HDC>(wParam));
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_SIZE:
		CancelDrag();
		RebuildProjection();
		return 0;
	case WM_DPICHANGED:
	case WM_DPICHANGED_AFTERPARENT:
		CancelDrag();
		m_dpi = GetDpiForWindow(m_window);
		if (!m_dpi) m_dpi = 96;
		RebuildProjection();
		return 0;
	case WM_ENABLE:
		if (!wParam) CancelDrag();
		InvalidateRect(m_window, nullptr, FALSE);
		return 0;
	case WM_SYSCOLORCHANGE:
	case WM_THEMECHANGED:
	case WM_SETFOCUS:
		InvalidateRect(m_window, nullptr, FALSE);
		return 0;
	case WM_KILLFOCUS:
	case WM_CANCELMODE:
	case WM_CAPTURECHANGED:
		CancelDrag();
		InvalidateRect(m_window, nullptr, FALSE);
		return 0;
	case WM_GETDLGCODE:
	{
		const MSG* key = reinterpret_cast<const MSG*>(lParam);
		const bool escape = m_pressed && key && key->wParam == VK_ESCAPE;
		return DLGC_WANTARROWS | DLGC_WANTCHARS | (escape ? DLGC_WANTMESSAGE : 0);
	}
	case WM_KEYDOWN:
		HandleKey(static_cast<UINT>(wParam));
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	{
		const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
		const size_t index = HitTest(point);
		SetFocus(m_window);
		if (index == NoMonitor) return 0;
		Select(index);
		if (!m_window || index >= m_count || index >= m_cache.size() || m_selected != index) return 0;
		m_pressed = true;
		m_dragging = false;
		m_dragIndex = index;
		m_dragStart = point;
		m_dragOriginal = m_items[index].desktop;
		m_dragPosition = {m_dragOriginal.left, m_dragOriginal.top};
		m_dragBox = m_cache[index].box;
		SetCapture(m_window);
		return 0;
	}
	case WM_MOUSEMOVE:
		UpdateDrag({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
		return 0;
	case WM_LBUTTONUP:
		if (m_pressed) UpdateDrag({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
		FinishDrag();
		return 0;
	case WM_RBUTTONUP:
	{
		POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
		ClientToScreen(m_window, &point);
		ShowContextMenu(point);
		return 0;
	}
	case WM_CONTEXTMENU:
		ShowContextMenu({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
		return 0;
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT)
		{
			POINT point;
			GetCursorPos(&point);
			ScreenToClient(m_window, &point);
			SetCursor(LoadCursorW(nullptr, m_pressed || HitTest(point) != NoMonitor ? IDC_SIZEALL : IDC_ARROW));
			return TRUE;
		}
		break;
	}
	return DefSubclassProc(m_window, message, wParam, lParam);
}
