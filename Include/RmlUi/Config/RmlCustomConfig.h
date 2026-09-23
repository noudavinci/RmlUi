#pragma once

/*
 * RmlCustomConfig.h — CUSTOM CONFIGURATION của RmlUi (thay thế <RmlUi/Config/Config.h>)
 * =====================================================================================
 * File này được include thay cho Config.h nhờ CMake option:
 *   RMLUI_CUSTOM_CONFIGURATION ON
 *   RMLUI_CUSTOM_CONFIGURATION_FILE <đường dẫn tuyệt đối tới file này>
 *
 * MỤC ĐÍCH: đưa mọi container chuẩn của RmlUi (Vector/String/Map/Set/...) vào
 * SlabPool zero-heap của dự án (UISlabPool) thay vì glibc heap. Toàn bộ DOM,
 * layout, computed style, render-manager mesh sẽ được cấp phát từ pool riêng.
 *
 * CƠ CHẾ (giữ phân tầng — fork KHÔNG link trực tiếp code dự án):
 *   RmlAllocator<T> dưới đây ủy quyền cấp phát qua 2 HÀM `extern "C"`:
 *     void *rml_ui_alloc(size_t bytes, size_t alignment);
 *     void  rml_ui_free (void *ptr, size_t bytes, size_t alignment);
 *   Hai hàm này được ĐỊNH NGHĨA ở phía dự án (NdvcUtils::RmlUi::RmlUiAlloc)
 *   và trỏ tới UISlabPool — đăng ký TRƯỚC Rml::Initialise().
 *   Nếu chưa đăng ký (trước Initialise hoặc lúc static init), RmlAllocator quay
 *   về malloc/free nên RmlUi vẫn hoạt động an toàn.
 *
 * ABI/ODR: file này được include bởi CẢ librmlui.so lẫn consumer (module ui) qua
 * macro RMLUI_CUSTOM_CONFIGURATION_FILE (compile definition PUBLIC trên
 * rmlui_core). Kiểu container do đó KHỚP CHÍNH XÁC giữa .so và consumer.
 *
 * QUY ƯỚC DỰ ÁN: comment/log tiếng Việt; external/ được phép dùng std::function.
 */

#include <cstddef>
#include <cstdlib> // std::malloc / std::free
#include <new>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <array>
#include <deque>
#include <list>
#include <queue>
#include <stack>
#include <map>
#include <unordered_map> // cũng cung cấp std::unordered_multimap
#include <set>
#include <unordered_set>
#include <utility>

// Hooks cấp phát — định nghĩa ở phía dự án (NdvcUtils::RmlUi) trỏ UISlabPool.
// Khai báo extern "C" để symbol ổn định qua biên giới shared-lib (thư viện
// dựng bởi add_subdirectory + consumer dựng bởi dự án).
extern "C" {
void *rml_ui_alloc(size_t bytes, size_t alignment);
void rml_ui_free(void *ptr, size_t bytes, size_t alignment);
} // extern "C"

namespace Rml {

	// Default matrix type — bắt buộc giống Config.h gốc (Core/Types.h dùng
	// RMLUI_MATRIX4_TYPE để alias Matrix4f). Mặc định column-major.
	#ifdef RMLUI_MATRIX_ROW_MAJOR
		#define RMLUI_MATRIX4_TYPE RowMajorMatrix4f
	#else
		#define RMLUI_MATRIX4_TYPE ColumnMajorMatrix4f
	#endif

	// Cách vô hiệu 'final' cho Rml::Releaser (phá EASTL nếu bật).
	#define RMLUI_RELEASER_FINAL final

// =============================================================================
// RmlAllocator<T> — allocator chuẩn C++20 ủy quyền qua 2 hàm extern "C" trỏ
// UISlabPool. Stateless (mọi instance equal) để tương thích STL containers.
// =============================================================================
template <typename T>
class RmlAllocator {
public:
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::true_type;

	constexpr RmlAllocator() noexcept = default;
	template <typename U>
	constexpr RmlAllocator(const RmlAllocator<U>&) noexcept {}
	~RmlAllocator() = default;

	T* allocate(std::size_t n) {
		if (n == 0)
			return nullptr;
		// Kiểm tra overflow nhân kích thước — phòng lỗi tính toán kích thước.
		if (n > size_type(-1) / sizeof(T))
			throw std::bad_alloc();
		void* p = rml_ui_alloc(n * sizeof(T), alignof(T));
		if (!p)
			throw std::bad_alloc();
		return static_cast<T*>(p);
	}

	void deallocate(T* p, std::size_t n) noexcept {
		if (p)
			rml_ui_free(p, n * sizeof(T), alignof(T));
	}

	template <typename U>
	struct rebind {
		using other = RmlAllocator<U>;
	};

	friend bool operator==(const RmlAllocator&, const RmlAllocator&) noexcept { return true; }
	friend bool operator!=(const RmlAllocator&, const RmlAllocator&) noexcept { return false; }
};

// =============================================================================
// Các container types — giữ NGUYÊN cấu trúc alias của Config.h gốc, chỉ gắn
// RmlAllocator để toàn bộ đi qua UISlabPool.
// =============================================================================
template <typename T>
using Vector = std::vector<T, RmlAllocator<T>>;
template <typename T, size_t N = 1>
using Array = std::array<T, N>;
template <typename T>
using Stack = std::stack<T, std::deque<T, RmlAllocator<T>>>;
template <typename T>
using List = std::list<T, RmlAllocator<T>>;
template <typename T>
using Queue = std::queue<T, std::deque<T, RmlAllocator<T>>>;
template <typename T1, typename T2>
using Pair = std::pair<T1, T2>;
template <typename Key, typename Value>
using StableMap = std::map<Key, Value, std::less<Key>, RmlAllocator<std::pair<const Key, Value>>>;
template <typename Key, typename Value>
using StableUnorderedMap = std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, RmlAllocator<std::pair<const Key, Value>>>;
template <typename Key, typename Value>
using UnorderedMultimap = std::unordered_multimap<Key, Value, std::hash<Key>, std::equal_to<Key>, RmlAllocator<std::pair<const Key, Value>>>;

// Bật RMLUI_NO_THIRDPARTY_CONTAINERS để bỏ robin_hood/itlib (gọi std::malloc
// trực tiếp) — rơi về std::unordered_map/pmr ủy quyền UISlabPool.
template <typename Key, typename Value>
using UnorderedMap = std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, RmlAllocator<std::pair<const Key, Value>>>;
template <typename Key, typename Value>
using SmallUnorderedMap = UnorderedMap<Key, Value>;
template <typename Key, typename Value>
using SmallOrderedMap = StableMap<Key, Value>;
template <typename T>
using UnorderedSet = std::unordered_set<T, std::hash<T>, std::equal_to<T>, RmlAllocator<T>>;
template <typename T>
using SmallUnorderedSet = UnorderedSet<T>;
template <typename T>
using SmallOrderedSet = std::set<T, std::less<T>, RmlAllocator<T>>;

// Utilities.
template <typename T>
using Hash = std::hash<T>;
template <typename T>
using Function = std::function<T>; // SBO-strategy: giữ std::function (external/; rà Giai đoạn 5)
template <typename Iterator>
inline std::move_iterator<Iterator> MakeMoveIterator(Iterator it)
{
	return std::make_move_iterator(it);
}

// Strings.
using String = std::basic_string<char, std::char_traits<char>, RmlAllocator<char>>;
using StringList = Vector<String>;

// Smart pointer types.
template <typename T>
using UniquePtr = std::unique_ptr<T>;
template <typename T>
class Releaser;
template <typename T>
using UniqueReleaserPtr = std::unique_ptr<T, Releaser<T>>;
template <typename T>
using SharedPtr = std::shared_ptr<T>;
template <typename T>
using WeakPtr = std::weak_ptr<T>;
template <typename T, typename... Args>
inline SharedPtr<T> MakeShared(Args&&... args)
{
	// Control block vẫn đi glibc (std::make_shared) — ghi chú Giai đoạn 5.
	return std::make_shared<T, Args...>(std::forward<Args>(args)...);
}
template <typename T, typename... Args>
inline UniquePtr<T> MakeUnique(Args&&... args)
{
	return std::make_unique<T, Args...>(std::forward<Args>(args)...);
}

} // namespace Rml
