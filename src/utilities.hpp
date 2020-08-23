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
        return *this;
    }

    EnumerateIterator& operator=(EnumerateIterator&& rhs)
    {
        mIterator = std::move(rhs.mIterator);
        mCounter = std::move(rhs.mCounter);
        return *this;
    }

    void ensure_data_has_been_fetched() const
    {
        if (!mData.has_value())
            mData = std::make_tuple(mCounter, *mIterator);
    }

    EnumerateIterator& operator++()
    {
        ++mIterator;
        ++mCounter;
        return *this;
    }

    EnumerateIterator operator++(int) const
    {
        ++mIterator;
        return EnumerateIterator(mIterator, mCounter + 1);
    }

    EnumerateIterator& operator+=(difference_type count)
    {
        mIterator += count;
        mCounter += count;
        return *this;
    }

    EnumerateIterator& operator--()
    {
        --mIterator;
        --mCounter;
        return *this;
    }

    EnumerateIterator operator--(int) const
    {
        --mIterator;
        return EnumerateIterator(mIterator, mCounter - 1);
    }

    EnumerateIterator operator-=(difference_type count)
    {
        mIterator -= count;
        mCounter -= count;
        return *this;
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
