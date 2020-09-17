#pragma once

#include <cstddef>
#include <utility>
#include <iterator>
#include <optional>

template<typename IteratorA, typename IteratorB>
struct ZipIterator
{
    IteratorA                 mIteratorA;
    IteratorB                 mIteratorB;

    // NOTE(andrea.machizaud) arbitrary pick IteratorA as reference
    // using iterator_category = typename IteratorA::iterator_category;
    using value_type        = std::tuple<decltype(*mIteratorA), decltype(*mIteratorB)>;
    using difference_type   = decltype(std::distance(mIteratorA, mIteratorA + 1));
    using pointer           = value_type*;
    using const_pointer     = const value_type*;
    using reference         = value_type;
    using const_reference   = const value_type;

    ZipIterator(IteratorA iteratorA, IteratorB iteratorB)
        : mIteratorA(iteratorA)
        , mIteratorB(iteratorB)
    {
    }

    ZipIterator(const ZipIterator& rhs)
        : mIteratorA(rhs.mIteratorA)
        , mIteratorB(rhs.mIteratorB)
    {
    }

    ZipIterator(ZipIterator&& rhs)
        : mIteratorA(std::move(rhs.mIteratorA))
        , mIteratorB(std::move(rhs.mIteratorB))
    {
    }

    ZipIterator& operator=(const ZipIterator& rhs)
    {
        mIteratorA = rhs.mIteratorA;
        mIteratorB = rhs.mIteratorB;
        return *this;
    }

    ZipIterator& operator=(ZipIterator&& rhs)
    {
        mIteratorA = std::move(rhs.mIteratorA);
        mIteratorB = std::move(rhs.mIteratorB);
        return *this;
    }

    ZipIterator& operator++()
    {
        ++mIteratorA;
        ++mIteratorB;
        return *this;
    }

    ZipIterator operator++(int) const
    {
        return ZipIterator(mIteratorA + 1, mIteratorB + 1);
    }

    ZipIterator& operator+=(difference_type count)
    {
        mIteratorA += count;
        mIteratorB += count;
        return *this;
    }

    ZipIterator& operator--()
    {
        --mIteratorA;
        --mIteratorB;
        return *this;
    }

    ZipIterator operator--(int) const
    {
        return ZipIterator(mIteratorA - 1, mIteratorB - 1);
    }

    ZipIterator operator-=(difference_type count)
    {
        mIteratorA -= count;
        mIteratorB -= count;
        return *this;
    }

    reference operator*()
    {
        return std::tie(*mIteratorA, *mIteratorB);
    }

    const_reference operator*() const
    {
        return std::tie(*mIteratorA, *mIteratorB);
    }
};

// NOTE(andrea.machizaud) both needs to match
template<typename IteratorA, typename IteratorB>
bool operator==(const ZipIterator<IteratorA, IteratorB>& lhs, const ZipIterator<IteratorA, IteratorB>& rhs)
{
    return (lhs.mIteratorA == rhs.mIteratorA) && (lhs.mIteratorB == rhs.mIteratorB);
}

// NOTE(andrea.machizaud) only one match needed : first to reach end, will return true
template<typename IteratorA, typename IteratorB>
bool operator!=(const ZipIterator<IteratorA, IteratorB>& lhs, const ZipIterator<IteratorA, IteratorB>& rhs)
{
    return (lhs.mIteratorA != rhs.mIteratorA) || (lhs.mIteratorB != rhs.mIteratorB);
}

template<typename IteratorA, typename IteratorB>
typename ZipIterator<IteratorA, IteratorB>::difference_type operator+(const ZipIterator<IteratorA, IteratorB>& lhs, const ZipIterator<IteratorA, IteratorB>& rhs)
{
    return lhs.mIterator + rhs.mIterator;
}

template<typename IteratorA, typename IteratorB>
typename ZipIterator<IteratorA, IteratorB>::difference_type operator-(const ZipIterator<IteratorA, IteratorB>& lhs, const ZipIterator<IteratorA, IteratorB>& rhs)
{
    return lhs.mIterator - rhs.mIterator;
}

template<typename IteratorA, typename IteratorB>
auto operator<=>(const ZipIterator<IteratorA, IteratorB>& lhs, const ZipIterator<IteratorA, IteratorB>& rhs)
{
    if ((lhs.mIteratorA <=> rhs.mIteratorA) == 0)
        return (lhs.mIteratorB <=> rhs.mIteratorB);
    else
        return 0;
}

template<typename ContainerA, typename ContainerB>
struct MutableZipRange
{
    using Iterator = ZipIterator<typename ContainerA::iterator, typename ContainerB::iterator>;

    Iterator mStart, mEnd;

    explicit MutableZipRange(ContainerA& containerA, ContainerB& containerB)
        : mStart(std::begin(containerA), std::begin(containerB))
        , mEnd(std::end(containerA), std::end(containerB))
    {
    }

    Iterator begin() { return mStart; }
    const Iterator cbegin() const { return mStart; }
    Iterator end() { return mEnd; }
    const Iterator cend() const { return mEnd; }
};

template<typename ContainerA, typename ContainerB>
struct ConstZipRange
{
    using Iterator = ZipIterator<typename ContainerA::const_iterator, typename ContainerB::const_iterator>;

    Iterator mStart, mEnd;

    explicit ConstZipRange(const ContainerA& containerA, const ContainerB& containerB)
        : mStart(std::begin(containerA), std::begin(containerB))
        , mEnd(std::end(containerA), std::end(containerB))
    {
    }

    Iterator begin() { return mStart; }
    const Iterator cbegin() const { return mStart; }
    Iterator end() { return mEnd; }
    const Iterator cend() const { return mEnd; }
};

template<typename ContainerA, typename ContainerB>
auto zip(const ContainerA& containerA, const ContainerB& containerB)
{
    return ConstZipRange(containerA, containerB);
}

template<typename ContainerA, typename ContainerB>
auto zip(ContainerA& containerA, ContainerB& containerB)
{
    return MutableZipRange(containerA, containerB);
}

template<typename ContainerA, typename ContainerB>
auto zip(ContainerA&& containerA, ContainerB&& containerB)
{
    return MutableZipRange(containerA, containerB);
}
