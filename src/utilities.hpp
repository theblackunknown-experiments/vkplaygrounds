#pragma once

#include <cstddef>
#include <utility>
#include <iterator>
#include <optional>

template<typename Iterator, typename CounterType = std::uint32_t>
struct EnumerateIterator
{
    using iterator_category = typename Iterator::iterator_category;
    using value_type        = std::tuple<CounterType, typename Iterator::value_type>;
    using difference_type   = typename Iterator::difference_type;
    using pointer           = value_type*;
    using reference         = value_type&;

    Iterator                  mIterator;
    CounterType               mCounter;
    // TODO(andrea.machizaud) we may relax this requirement dependending on the iterator category/user input
    // to satisfy operator* which returns a reference, we need to fetch data from iterator, however we delay this operatiom
    mutable std::optional<value_type> mData;

    EnumerateIterator(Iterator iterator, CounterType start = {})
        : mIterator(iterator)
        , mCounter(start)
    {
    }

    EnumerateIterator(const EnumerateIterator& rhs)
        : mIterator(rhs.mIterator)
        , mCounter(rhs.mCounter)
    {
    }

    EnumerateIterator(EnumerateIterator&& rhs)
        : mIterator(std::move(rhs.mIterator))
        , mCounter(std::move(rhs.mCounter))
    {
    }

    EnumerateIterator& operator=(const EnumerateIterator& rhs)
    {
        mIterator = rhs.mIterator;
        mCounter = rhs.mCounter;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator& operator=(EnumerateIterator&& rhs)
    {
        mIterator = std::move(rhs.mIterator);
        mCounter = std::move(rhs.mCounter);
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator& operator++()
    {
        ++mIterator;
        ++mCounter;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator operator++(int) const
    {
        return EnumerateIterator(mIterator + 1, mCounter + 1);
    }

    EnumerateIterator& operator+=(difference_type count)
    {
        mIterator += count;
        mCounter += count;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator& operator--()
    {
        --mIterator;
        --mCounter;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator operator--(int) const
    {
        return EnumerateIterator(mIterator - 1, mCounter - 1);
    }

    EnumerateIterator& operator-=(difference_type count)
    {
        mIterator -= count;
        mCounter -= count;
        reset_data_fetch();
        return *this;
    }

    void reset_data_fetch()
    {
        mData.reset();
    }

    void ensure_data_has_been_fetched() const
    {
        if (!mData.has_value())
            mData = std::make_tuple(mCounter, *mIterator);
    }

    reference operator*() const
    {
        ensure_data_has_been_fetched();
        return *mData;
    }
};

template<typename Iterator, typename CounterType>
bool operator==(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator == rhs.mIterator;
}

template<typename Iterator, typename CounterType>
bool operator!=(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return !(lhs == rhs);
}

template<typename Iterator, typename CounterType>
typename EnumerateIterator<Iterator, CounterType>::difference_type operator+(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator + rhs.mIterator;
}

template<typename Iterator, typename CounterType>
typename EnumerateIterator<Iterator, CounterType>::difference_type operator-(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator - rhs.mIterator;
}

template<typename Iterator, typename CounterType>
auto operator<=>(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator <=> rhs.mIterator;
}

template<typename Container, typename CounterType = std::uint32_t>
struct EnumerateRange
{
    using Iterator = EnumerateIterator<typename Container::iterator, CounterType>;

    Iterator mStart, mEnd;

    explicit EnumerateRange(Container& container, CounterType start = {})
        : mStart(std::begin(container), start)
        , mEnd(std::end(container), {})
    {
    }

    Iterator begin() { return mStart; }
    const Iterator cbegin() const { return mStart; }
    Iterator end() { return mEnd; }
    const Iterator cend() const { return mEnd; }
};

template<typename IteratorA, typename IteratorB>
struct ZipIterator
{
    // NOTE(andrea.machizaud) arbitrary pick IteratorA as reference
    using iterator_category = typename IteratorA::iterator_category;
    using value_type        = std::tuple<typename IteratorA::reference, typename IteratorB::reference>;
    using difference_type   = typename IteratorA::difference_type;
    using pointer           = value_type*;
    using const_pointer     = const value_type*;
    using reference         = value_type;
    using const_reference   = const value_type;

    IteratorA                 mIteratorA;
    IteratorB                 mIteratorB;
    // TODO(andrea.machizaud) we may relax this requirement dependending on the iterator category/user input
    // to satisfy operator* which returns a reference, we need to fetch data from iterator, however we delay this operatiom
    mutable std::optional<value_type> mData;

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
        return (lhs.mIteratorB <=> rhs.mIteratorB)
    else
        return 0;
}

template<typename ContainerA, typename ContainerB>
struct ZipRange
{
    using Iterator = ZipIterator<typename ContainerA::iterator, typename ContainerB::iterator>;

    Iterator mStart, mEnd;

    explicit ZipRange(ContainerA& containerA, ContainerB& containerB)
        : mStart(std::begin(containerA), std::begin(containerB))
        , mEnd(std::end(containerA), std::end(containerB))
    {
    }

    Iterator begin() { return mStart; }
    const Iterator cbegin() const { return mStart; }
    Iterator end() { return mEnd; }
    const Iterator cend() const { return mEnd; }
};
