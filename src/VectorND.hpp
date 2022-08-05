#ifndef SRC_VECTORND_HPP
#define SRC_VECTORND_HPP


#include <etl/type_traits.h>


namespace internal {
	template < class TComponentType >
	struct Vector2 {
		using ComponentType = TComponentType;

		constexpr Vector2 () = default;

		constexpr Vector2 (ComponentType i_x, ComponentType i_y)
		    : x{i_x}
		    , y{i_y}
		{
		}


		ComponentType x{};
		ComponentType y{};
	};


	template < class TComponentType >
	struct Vector3 {
		using ComponentType = TComponentType;

		constexpr Vector3 () = default;

		constexpr Vector3 (
		    ComponentType i_x, ComponentType i_y, ComponentType i_z)
		    : x{i_x}
		    , y{i_y}
		    , z{i_z}
		{
		}


		ComponentType x{};
		ComponentType y{};
		ComponentType z{};
	};
} // namespace internal


template <
    size_t KDim,
    class TComponentType,
    class TBaseType = etl::conditional_t<
        KDim == 2,
        internal::Vector2< TComponentType >,
        internal::Vector3< TComponentType > > >
struct VectorND : public TBaseType {
	static_assert (
	    (KDim == 2) || (KDim == 3), "Only 2D and 3D vectors are supported");


	using BaseType      = TBaseType;
	using ComponentType = TComponentType;

	static auto constexpr Dimensions = KDim;

	using SelfType = VectorND< Dimensions, ComponentType, BaseType >;

	static_assert (
	    etl::is_arithmetic_v< ComponentType >,
	    "Only arithmetic types are valid as vector component type.");

	using BaseType::BaseType;


	constexpr SelfType operator- () const noexcept
	{
		if constexpr (2 == Dimensions)
		{
			return SelfType{-BaseType::x, -BaseType::y};
		}
		else
		{
			return SelfType{-BaseType::x, -BaseType::y, -BaseType::z};
		}
	}


	ComponentType& operator[] (char i_axis_char) noexcept
	{
		static auto kInvalidAxis = ComponentType{};

		switch (i_axis_char)
		{
		default: {
			kInvalidAxis = ComponentType{};

			return kInvalidAxis;
		}

		case 'x':
			[[fallthrough]];
		case 'X': {
			return BaseType::x;
		}

		case 'y':
			[[fallthrough]];
		case 'Y': {
			return BaseType::y;
		}

		case 'z':
			[[fallthrough]];
		case 'Z': {
			if constexpr (3 == Dimensions)
			{
				return BaseType::z;
			}
			else
			{
				return kInvalidAxis;
			}
		}
		}
	}


	constexpr ComponentType operator[] (char i_axis_char) const noexcept
	{
		return const_cast< SelfType& > (*this)[ i_axis_char ];
	}
};


template < class TComponentType >
using Vector2 = VectorND< 2, TComponentType >;

using Vector2i = Vector2< int >;
using Vector2f = Vector2< float >;
using Vector2d = Vector2< double >;

template < class TComponentType >
using Vector3 = VectorND< 3, TComponentType >;

using Vector3f = Vector3< float >;
using Vector3d = Vector3< double >;


#define VECTORND_VECTOR_TYPE_COMPATIBILITY_CHECK                               \
	size_t KDim                 = TVectorND::Dimensions,                       \
	       class TComponentType = typename TVectorND::ComponentType,           \
	       class                = etl::enable_if_t <                           \
	        etl::is_same_v< VectorND< KDim, TComponentType >, TVectorND > ||   \
	    etl::is_base_of_v< VectorND< KDim, TComponentType >, TVectorND > >


#define VECTORND_VECTOR_VECTOR_COMPOUND_ASSIGNMENT_OPERATOR(i_op)              \
	template < class TVectorND, VECTORND_VECTOR_TYPE_COMPATIBILITY_CHECK >     \
	constexpr TVectorND operator i_op##= (                                     \
	    TVectorND& io_lhs, TVectorND const& i_rhs) noexcept                    \
	{                                                                          \
		if constexpr (2 == KDim)                                               \
		{                                                                      \
			io_lhs.x i_op## = i_rhs.x;                                         \
			io_lhs.y i_op## = i_rhs.y;                                         \
		}                                                                      \
		else                                                                   \
		{                                                                      \
			io_lhs.x i_op## = i_rhs.x;                                         \
			io_lhs.y i_op## = i_rhs.y;                                         \
			io_lhs.z i_op## = i_rhs.z;                                         \
		}                                                                      \
                                                                               \
		return io_lhs;                                                         \
	}


#define VECTORND_VECTOR_SCALAR_COMPOUND_ASSIGNMENT_OPERATOR(i_op)              \
	template <                                                                 \
	    class TVectorND,                                                       \
	    class TScalar,                                                         \
	    VECTORND_VECTOR_TYPE_COMPATIBILITY_CHECK >                             \
	constexpr TVectorND operator i_op##= (                                     \
	    TVectorND& io_lhs, TScalar const i_rhs) noexcept                       \
	{                                                                          \
		if constexpr (2 == KDim)                                               \
		{                                                                      \
			io_lhs.x i_op## = i_rhs;                                           \
			io_lhs.y i_op## = i_rhs;                                           \
		}                                                                      \
		else                                                                   \
		{                                                                      \
			io_lhs.x i_op## = i_rhs;                                           \
			io_lhs.y i_op## = i_rhs;                                           \
			io_lhs.z i_op## = i_rhs;                                           \
		}                                                                      \
                                                                               \
		return io_lhs;                                                         \
	}


#define VECTORND_VECTOR_VECTOR_ARITHMETIC_BINARY_OPERATOR(i_op)                \
	template < class TVectorND, VECTORND_VECTOR_TYPE_COMPATIBILITY_CHECK >     \
	constexpr TVectorND operator i_op (                                        \
	    TVectorND i_lhs, TVectorND const& i_rhs) noexcept                      \
	{                                                                          \
		return i_lhs i_op## = i_rhs;                                           \
	}


#define VECTORND_VECTOR_SCALAR_ARITHMETIC_BINARY_OPERATOR(i_op)                \
	template <                                                                 \
	    class TVectorND,                                                       \
	    class TScalar,                                                         \
	    VECTORND_VECTOR_TYPE_COMPATIBILITY_CHECK >                             \
	constexpr TVectorND operator i_op (                                        \
	    TVectorND i_lhs, TScalar const i_rhs) noexcept                         \
	{                                                                          \
		return i_lhs i_op## = i_rhs;                                           \
	}


#define VECTORND_SCALAR_VECTOR_ARITHMETIC_BINARY_OPERATOR(i_op)                \
	template <                                                                 \
	    class TVectorND,                                                       \
	    class TScalar,                                                         \
	    VECTORND_VECTOR_TYPE_COMPATIBILITY_CHECK >                             \
	constexpr TVectorND operator i_op (                                        \
	    TScalar const i_lhs, TVectorND i_rhs) noexcept                         \
	{                                                                          \
		return i_rhs i_op## = i_lhs;                                           \
	}


VECTORND_VECTOR_VECTOR_COMPOUND_ASSIGNMENT_OPERATOR (+)
VECTORND_VECTOR_VECTOR_COMPOUND_ASSIGNMENT_OPERATOR (-)
VECTORND_VECTOR_VECTOR_COMPOUND_ASSIGNMENT_OPERATOR (*)
VECTORND_VECTOR_VECTOR_COMPOUND_ASSIGNMENT_OPERATOR (/)

VECTORND_VECTOR_SCALAR_COMPOUND_ASSIGNMENT_OPERATOR (+)
VECTORND_VECTOR_SCALAR_COMPOUND_ASSIGNMENT_OPERATOR (-)
VECTORND_VECTOR_SCALAR_COMPOUND_ASSIGNMENT_OPERATOR (*)
VECTORND_VECTOR_SCALAR_COMPOUND_ASSIGNMENT_OPERATOR (/)

VECTORND_VECTOR_VECTOR_ARITHMETIC_BINARY_OPERATOR (+)
VECTORND_VECTOR_VECTOR_ARITHMETIC_BINARY_OPERATOR (-)
VECTORND_VECTOR_VECTOR_ARITHMETIC_BINARY_OPERATOR (*)
VECTORND_VECTOR_VECTOR_ARITHMETIC_BINARY_OPERATOR (/)

VECTORND_VECTOR_SCALAR_ARITHMETIC_BINARY_OPERATOR (+)
VECTORND_VECTOR_SCALAR_ARITHMETIC_BINARY_OPERATOR (-)
VECTORND_VECTOR_SCALAR_ARITHMETIC_BINARY_OPERATOR (*)
VECTORND_VECTOR_SCALAR_ARITHMETIC_BINARY_OPERATOR (/)

VECTORND_SCALAR_VECTOR_ARITHMETIC_BINARY_OPERATOR (+)
VECTORND_SCALAR_VECTOR_ARITHMETIC_BINARY_OPERATOR (-)
VECTORND_SCALAR_VECTOR_ARITHMETIC_BINARY_OPERATOR (*)
VECTORND_SCALAR_VECTOR_ARITHMETIC_BINARY_OPERATOR (/)


#undef VECTORND_VECTOR_TYPE_COMPATIBILITY_CHECK
#undef VECTORND_VECTOR_VECTOR_COMPOUND_ASSIGNMENT_OPERATOR
#undef VECTORND_VECTOR_SCALAR_COMPOUND_ASSIGNMENT_OPERATOR
#undef VECTORND_VECTOR_VECTOR_ARITHMETIC_BINARY_OPERATOR
#undef VECTORND_VECTOR_SCALAR_ARITHMETIC_BINARY_OPERATOR
#undef VECTORND_SCALAR_VECTOR_ARITHMETIC_BINARY_OPERATOR


#endif // SRC_VECTORND_HPP
