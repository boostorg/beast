//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//
// This is a derivative work based on Zlib 1.3.2 (deflate.c, trees.c),
// copyright below:
/*
    Copyright (C) 1995-2026 Jean-loup Gailly and Mark Adler

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.

    Jean-loup Gailly        Mark Adler
    jloup@gzip.org          madler@alumni.caltech.edu

    The data format used by the zlib library is described by RFCs (Request for
    Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
    (zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).
*/

#ifndef BOOST_BEAST_ZLIB_DETAIL_DEFLATE_STREAM_IPP
#define BOOST_BEAST_ZLIB_DETAIL_DEFLATE_STREAM_IPP

#include <boost/beast/zlib/detail/deflate_stream.hpp>
#include <boost/beast/zlib/detail/ranges.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/make_unique.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace boost {
namespace beast {
namespace zlib {
namespace detail {

/*
 *  ALGORITHM
 *
 *      The "deflation" process depends on being able to identify portions
 *      of the input text which are identical to earlier input (within a
 *      sliding window trailing behind the input currently being processed).
 *
 *      Each code tree is stored in a compressed form which is itself
 *      a Huffman encoding of the lengths of all the code strings (in
 *      ascending order by source values).  The actual code strings are
 *      reconstructed from the lengths in the inflate process, as described
 *      in the deflate specification.
 *
 *      The most straightforward technique turns out to be the fastest for
 *      most input files: try all possible matches and select the longest.
 *      The key feature of this algorithm is that insertions into the string
 *      dictionary are very simple and thus fast, and deletions are avoided
 *      completely. Insertions are performed at each input character, whereas
 *      string matches are performed only when the previous match ends. So it
 *      is preferable to spend more time in matches to allow very fast string
 *      insertions and avoid deletions. The matching algorithm for small
 *      strings is inspired from that of Rabin & Karp. A brute force approach
 *      is used to find longer strings when a small match has been found.
 *      A similar algorithm is used in comic (by Jan-Mark Wams) and freeze
 *      (by Leonid Broukhis).
 *         A previous version of this file used a more sophisticated algorithm
 *      (by Fiala and Greene) which is guaranteed to run in linear amortized
 *      time, but has a larger average cost, uses more memory and is patented.
 *      However the F&G algorithm may be faster for some highly redundant
 *      files if the parameter max_chain_length (described below) is too large.
 *
 *  ACKNOWLEDGEMENTS
 *
 *      The idea of lazy evaluation of matches is due to Jan-Mark Wams, and
 *      I found it in 'freeze' written by Leonid Broukhis.
 *      Thanks to many people for bug reports and testing.
 *
 *  REFERENCES
 *
 *      Deutsch, L.P.,"DEFLATE Compressed Data Format Specification".
 *      Available at https://datatracker.ietf.org/doc/html/rfc1951
 *
 *      A description of the Rabin and Karp algorithm is given in the book
 *         "Algorithms" by R. Sedgewick, Addison-Wesley, p252.
 *
 *      Fiala,E.R., and Greene,D.H.
 *         Data Compression with Finite Windows, Comm.ACM, 32,4 (1989) 490-595
 *
 */

/*  Generate the codes for a given tree and bit counts (which need not be optimal).
    IN assertion: the array bl_count contains the bit length statistics for
       the given tree and the field len is set for all tree elements.
    OUT assertion: the field code is set for all tree elements of non
        zero code length.
*/
void
deflate_stream::
gen_codes(ct_data *tree, int max_code, std::uint16_t *bl_count)
{
    std::uint16_t next_code[maxBits+1]; /* next code value for each bit length */
    unsigned code = 0;         /* running code value */
    int bits;                  /* bit index */
    int n;                     /* code index */

    // The distribution counts are first used to
    // generate the code values without bit reversal.
    for(bits = 1; bits <= maxBits; bits++)
    {
        code = (code + bl_count[bits-1]) << 1;
        next_code[bits] = (std::uint16_t)code;
    }
    // Check that the bit counts in bl_count are consistent.
    // The last code must be all ones.
    BOOST_ASSERT(code + bl_count[maxBits]-1 == (1<<maxBits)-1);
    for(n = 0; n <= max_code; n++)
    {
        int len = tree[n].dl;
        if(len == 0)
            continue;
        tree[n].fc = bi_reverse(next_code[len]++, len);
    }
}

auto
deflate_stream::get_lut() ->
    lut_type const&
{
    struct init
    {
        lut_type tables;

        init()
        {
            // number of codes at each bit length for an optimal tree
            //std::uint16_t bl_count[maxBits+1];

            // Initialize the mapping length (0..255) -> length code (0..28)
            std::uint8_t length = 0;
            for(std::uint8_t code = 0; code < lengthCodes-1; ++code)
            {
                tables.base_length[code] = length;
                auto const run = 1U << tables.extra_lbits[code];
                for(unsigned n = 0; n < run; ++n)
                    tables.length_code[length++] = code;
            }
            BOOST_ASSERT(length == 0);
            // Note that the length 255 (match length 258) can be represented
            // in two different ways: code 284 + 5 bits or code 285, so we
            // overwrite length_code[255] to use the best encoding:
            tables.length_code[255] = lengthCodes-1;

            // Initialize the mapping dist (0..32K) -> dist code (0..29)
            {
                std::uint8_t code;
                std::uint16_t dist = 0;
                for(code = 0; code < 16; code++)
                {
                    tables.base_dist[code] = dist;
                    auto const run = 1U << tables.extra_dbits[code];
                    for(unsigned n = 0; n < run; ++n)
                        tables.dist_code[dist++] = code;
                }
                BOOST_ASSERT(dist == 256);
                // from now on, all distances are divided by 128
                dist >>= 7;
                for(; code < dCodes; ++code)
                {
                    tables.base_dist[code] = dist << 7;
                    auto const run = 1U << (tables.extra_dbits[code]-7);
                    for(std::size_t n = 0; n < run; ++n)
                        tables.dist_code[256 + dist++] = code;
                }
                BOOST_ASSERT(dist == 256);
            }

            // Construct the codes of the static literal tree
            std::uint16_t bl_count[maxBits+1];
            std::memset(bl_count, 0, sizeof(bl_count));
            unsigned n = 0;
            while (n <= 143)
                tables.ltree[n++].dl = 8;
            bl_count[8] += 144;
            while (n <= 255)
                tables.ltree[n++].dl = 9;
            bl_count[9] += 112;
            while (n <= 279)
                tables.ltree[n++].dl = 7;
            bl_count[7] += 24;
            while (n <= 287)
                tables.ltree[n++].dl = 8;
            bl_count[8] += 8;
            // Codes 286 and 287 do not exist, but we must include them in the tree
            // construction to get a canonical Huffman tree (longest code all ones)
            gen_codes(tables.ltree, lCodes+1, bl_count);

            for(n = 0; n < dCodes; ++n)
            {
                tables.dtree[n].dl = 5;
                tables.dtree[n].fc =
                    static_cast<std::uint16_t>(bi_reverse(n, 5));
            }
        }
    };
    static init const data;
    return data.tables;
}

void
deflate_stream::
doReset(
    int level,
    int windowBits,
    int memLevel,
    Strategy strategy)
{
    if(level == default_size)
        level = 6;

    if(level < 0 || level > 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid level"});

    /*  deflate does not support a 256-byte window. With the zlib
        wrapper, zlib silently uses a 512-byte window instead and
        advertises it in the header, but a raw deflate stream has
        no header, so zlib rejects windowBits == 8 for raw streams.
    */
    if(windowBits < 9 || windowBits > 15)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid windowBits"});

    if(memLevel < 1 || memLevel > max_mem_level)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid memLevel"});

    w_bits_ = windowBits;

    hash_bits_ = memLevel + 7;

    // 16K elements by default
    lit_bufsize_ = 1 << (memLevel + 6);

    level_ = level;
    strategy_ = strategy;
    inited_ = false;
}

void
deflate_stream::
doReset()
{
    inited_ = false;
}

void
deflate_stream::
doClear()
{
    inited_ = false;
    buf_.reset();
}

/*  For the default windowBits of 15 and memLevel of 8, this function returns a
    close to exact, as well as small, upper bound on the compressed size. This
    is an expansion of ~0.03%, plus a small constant.

    For any setting other than those defaults for windowBits and memLevel, one
    of two worst case bounds is returned. This is at most an expansion of ~4% or
    ~13%, plus a small constant.

    Both the 0.03% and 4% derive from the overhead of stored blocks. The first
    one is for stored blocks of 16383 bytes (memLevel == 8), whereas the second
    is for stored blocks of 127 bytes (the worst case memLevel == 1). The
    expansion results from five bytes of header for each stored block.

    The larger expansion of 13% results from a window size less than or equal to
    the symbols buffer size (windowBits <= memLevel + 7). In that case some of
    the data being compressed may have slid out of the sliding window, impeding
    a stored block from being emitted. Then the only choice is a fixed or
    dynamic block, where a fixed block limits the maximum expansion to 9 bits
    per 8-bit byte, plus 10 bits for every block. The smallest block size for
    which this can occur is 255 (memLevel == 2).

    Shifts are used to approximate divisions, for speed.
*/
std::size_t
deflate_stream::
doUpperBound(std::size_t sourceLen) const
{
    std::size_t fixedlen, storelen, bound;

    /* upper bound for fixed blocks with 9-bit literals and length 255
       (memLevel == 2, which is the lowest that may not use stored blocks) --
       ~13% overhead plus a small constant */
    fixedlen = sourceLen + (sourceLen >> 3) + (sourceLen >> 8) +
               (sourceLen >> 9) + 4;
    if(fixedlen < sourceLen)
        fixedlen = static_cast<std::size_t>(-1);

    /* upper bound for stored blocks with length 127 (memLevel == 1) --
       ~4% overhead plus a small constant */
    storelen = sourceLen + (sourceLen >> 5) + (sourceLen >> 7) +
               (sourceLen >> 11) + 7;
    if(storelen < sourceLen)
        storelen = static_cast<std::size_t>(-1);

    /* raw deflate: there is no wrapper */

    /* if not default parameters, return one of the conservative bounds */
    if(w_bits_ != 15 || hash_bits_ != 8 + 7)
        return w_bits_ <= hash_bits_ && level_ ? fixedlen : storelen;

    /* default settings: return tight bound for that case -- ~0.03% overhead
       plus a small constant */
    bound = sourceLen + (sourceLen >> 12) + (sourceLen >> 14) +
            (sourceLen >> 25) + 13 - 6;
    return bound < sourceLen ? static_cast<std::size_t>(-1) : bound;
}

void
deflate_stream::
doTune(
    int good_length,
    int max_lazy,
    int nice_length,
    int max_chain)
{
    maybe_init(); // lm_init() would otherwise overwrite these

    good_match_ = (uInt)good_length;
    max_lazy_match_ = (uInt)max_lazy;
    nice_match_ = nice_length;
    max_chain_length_ = (uInt)max_chain;
}

void
deflate_stream::
doParams(z_params& zs, int level, Strategy strategy, error_code& ec)
{
    compress_func func;

    ec = {};
    maybe_init();

    if(level == default_size)
        level = 6;
    if(level < 0 || level > 9)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::stream_error);
        return;
    }
    func = get_config(level_).func;

    if((strategy != strategy_ || func != get_config(level).func) &&
        last_flush_ != -2)
    {
        // Flush the last buffer:
        doWrite(zs, Flush::block, ec);
        if(ec == error::stream_error)
            return;
        ec = {};
        if(zs.avail_in || (strstart_ - block_start_) + lookahead_)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::need_buffers);
            return;
        }
    }
    if(level_ != level)
    {
        if(level_ == 0 && matches_ != 0)
        {
            if(matches_ == 1)
                slide_hash();
            else
                clear_hash();
            matches_ = 0;
        }
        level_ = level;
        max_lazy_match_   = get_config(level).max_lazy;
        good_match_       = get_config(level).good_length;
        nice_match_       = get_config(level).nice_length;
        max_chain_length_ = get_config(level).max_chain;
    }
    strategy_ = strategy;
}

void
deflate_stream::
doWrite(z_params& zs, Flush flush, error_code& ec)
{
    ec = {};
    maybe_init();

    if(flush == Flush::trees ||
        zs.next_out == nullptr ||
        (zs.next_in == nullptr && zs.avail_in != 0) ||
        (status_ == finish_state && flush != Flush::finish))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::stream_error);
        return;
    }
    if(zs.avail_out == 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_buffers);
        return;
    }

    // value of flush param for previous deflate call
    int const old_flush = last_flush_;
    last_flush_ = static_cast<int>(flush);

    // Flush as much pending output as possible
    if(pending_ != 0)
    {
        flush_pending(zs);
        if(zs.avail_out == 0)
        {
            /* Since avail_out is 0, deflate will be called again with
             * more output space, but possibly with both pending and
             * avail_in equal to zero. There won't be anything to do,
             * but this is not an error situation so make sure we
             * return OK instead of BUF_ERROR at next call of deflate:
             */
            last_flush_ = -1;
            return;
        }
    }
    else if(zs.avail_in == 0 &&
        static_cast<int>(flush) <= old_flush && // Caution: depends on enum order
        flush != Flush::finish)
    {
        /* Make sure there is something to do and avoid duplicate consecutive
         * flushes. For repeated and useless calls with Flush::finish, we keep
         * returning Z_STREAM_END instead of Z_BUF_ERROR.
         */
        BOOST_BEAST_ASSIGN_EC(ec, error::need_buffers);
        return;
    }

    // User must not provide more input after the first FINISH:
    if(status_ == finish_state && zs.avail_in != 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_buffers);
        return;
    }

    /* Start a new block or continue the current one.
     */
    if(zs.avail_in != 0 || lookahead_ != 0 ||
        (flush != Flush::none && status_ != finish_state))
    {
        block_state bstate;

        bstate = level_ == 0 ? deflate_stored(zs, flush) :
                 strategy_ == Strategy::huffman ? deflate_huff(zs, flush) :
                 strategy_ == Strategy::rle ? deflate_rle(zs, flush) :
                 (this->*(get_config(level_).func))(zs, flush);

        if(bstate == finish_started || bstate == finish_done)
        {
            status_ = finish_state;
        }
        if(bstate == need_more || bstate == finish_started)
        {
            if(zs.avail_out == 0)
            {
                last_flush_ = -1; /* avoid BUF_ERROR next call, see above */
            }
            return;
            /*  If flush != Flush::none && avail_out == 0, the next call
                of deflate should use the same flush parameter to make sure
                that the flush is complete. So we don't have to output an
                empty block here, this will be done at next call. This also
                ensures that for a very small output buffer, we emit at most
                one empty block.
            */
        }
        if(bstate == block_done)
        {
            if(flush == Flush::partial)
            {
                tr_align();
            }
            else if(flush != Flush::block)
            {
                /* FULL_FLUSH or SYNC_FLUSH */
                tr_stored_block(nullptr, 0L, 0);
                /* For a full flush, this empty block will be recognized
                 * as a special marker by inflate_sync().
                 */
                if(flush == Flush::full)
                {
                    clear_hash(); // forget history
                    if(lookahead_ == 0)
                    {
                        strstart_ = 0;
                        block_start_ = 0L;
                        insert_ = 0;
                    }
                }
            }
            flush_pending(zs);
            if(zs.avail_out == 0)
            {
                last_flush_ = -1; /* avoid BUF_ERROR at next call, see above */
                return;
            }
        }
    }

    if(flush == Flush::finish)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::end_of_stream);
        return;
    }
}

void
deflate_stream::
doPrime(int bits, int value, error_code& ec)
{
    ec = {};
    maybe_init();

    if(bits < 0 || bits > 16 ||
        sym_buf_ < pending_out_ + ((Buf_size + 7) >> 3))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_buffers);
        return;
    }

    do
    {
        int put = Buf_size - bi_valid_;
        if(put > bits)
            put = bits;
        bi_buf_ |= (std::uint16_t)((value & ((1 << put) - 1)) << bi_valid_);
        bi_valid_ += put;
        tr_flush_bits();
        value >>= put;
        bits -= put;
    }
    while(bits);
}

void
deflate_stream::
doPending(unsigned* value, int* bits)
{
    maybe_init();

    if(value != 0)
        *value = pending_;
    if(bits != 0)
        *bits = bi_valid_;
}

//--------------------------------------------------------------------------

// Do lazy initialization
void
deflate_stream::
init()
{
    //  Caller must set these:
    //      w_bits_
    //      hash_bits_
    //      lit_bufsize_
    //      level_
    //      strategy_

    w_size_ = 1 << w_bits_;
    w_mask_ = w_size_ - 1;

    hash_size_ = 1 << hash_bits_;
    hash_mask_ = hash_size_ - 1;
    hash_shift_ =  ((hash_bits_+minMatch-1)/minMatch);

    auto const nwindow  = w_size_ * 2*sizeof(Byte);
    auto const nprev    = w_size_ * sizeof(std::uint16_t);
    auto const nhead    = hash_size_ * sizeof(std::uint16_t);
    auto const noverlay = lit_bufsize_ * (sizeof(std::uint16_t)+2);
    auto const needed   = nwindow + nprev + nhead + noverlay;

    if(! buf_ || buf_size_ != needed)
    {
        buf_ = boost::make_unique_noinit<
            std::uint8_t[]>(needed);
        buf_size_ = needed;
    }

    window_ = reinterpret_cast<Byte*>(buf_.get());
    prev_   = reinterpret_cast<std::uint16_t*>(buf_.get() + nwindow);
    std::memset(prev_, 0, nprev);
    head_   = reinterpret_cast<std::uint16_t*>(buf_.get() + nwindow + nprev);

    // nothing written to window_ yet
    high_water_ = 0;


    /* We overlay pending_buf and sym_buf. This works since the average size
       for length/distance pairs over any compressed block is assured to be 31
       bits or less.

       Analysis: The longest fixed codes are a length code of 8 bits plus 5
       extra bits, for lengths 131 to 257. The longest fixed distance codes are
       5 bits plus 13 extra bits, for distances 16385 to 32768. The longest
       possible fixed-codes length/distance pair is then 31 bits total.

       sym_buf starts one-fourth of the way into pending_buf. So there are
       three bytes in sym_buf for every four bytes in pending_buf. Each symbol
       in sym_buf is three bytes -- two for the distance and one for the
       literal/length. As each symbol is consumed, the pointer to the next
       sym_buf value to read moves forward three bytes. From that symbol, up to
       31 bits are written to pending_buf. The closest the written pending_buf
       bits gets to the next sym_buf symbol to read is just before the last
       code is written. At that time, 31*(n-2) bits have been written, just
       after 24*(n-2) bits have been consumed from sym_buf. sym_buf starts at
       8*n bits into pending_buf. (Note that the symbol buffer fills when n-1
       symbols are written.) The closest the writing gets to what is unread is
       then n+14 bits. Here n is lit_bufsize, which is 16384 by default, and
       can range from 128 to 32768.

       Therefore, at a minimum, there are 142 bits of space between what is
       written and what is read in the overlain buffers, so the symbols cannot
       be overwritten by the compressed data. That space is actually 139 bits,
       due to the three-bit fixed-code block header.

       That covers the case where either Z_FIXED is specified, forcing fixed
       codes, or when the use of fixed codes is chosen, because that choice
       results in a smaller compressed block than dynamic codes. That latter
       condition then assures that the above analysis also covers all dynamic
       blocks. A dynamic-code block will only be chosen to be emitted if it has
       fewer bits than a fixed-code block would for the same set of symbols.
       Therefore its average symbol length is assured to be less than 31. So
       the compressed data for a dynamic block also cannot overwrite the
       symbols from which it is being constructed.
     */
    pending_buf_ =
        buf_.get() + nwindow + nprev + nhead;
    pending_buf_size_ =
        static_cast<std::uint32_t>(lit_bufsize_) * 4;

    sym_buf_ = pending_buf_ + lit_bufsize_;
    sym_end_ = (lit_bufsize_ - 1) * 3;

    pending_ = 0;
    pending_out_ = pending_buf_;

    status_ = busy_state;
    last_flush_ = -2; // no call to write yet

    tr_init();
    lm_init();

    inited_ = true;
}

/*  Initialize the "longest match" routines for a new zlib stream
*/
void
deflate_stream::
lm_init()
{
    window_size_ = (std::uint32_t)2L*w_size_;

    clear_hash();

    /* Set the default configuration parameters:
     */
    // VFALCO TODO just copy the config struct
    max_lazy_match_   = get_config(level_).max_lazy;
    good_match_       = get_config(level_).good_length;
    nice_match_       = get_config(level_).nice_length;
    max_chain_length_ = get_config(level_).max_chain;

    strstart_ = 0;
    block_start_ = 0L;
    lookahead_ = 0;
    insert_ = 0;
    match_length_ = prev_length_ = minMatch-1;
    match_available_ = 0;
    ins_h_ = 0;
}

// Initialize a new block.
//
void
deflate_stream::
init_block()
{
    for(int n = 0; n < lCodes;  n++)
        dyn_ltree_[n].fc = 0;
    for(int n = 0; n < dCodes;  n++)
        dyn_dtree_[n].fc = 0;
    for(int n = 0; n < blCodes; n++)
        bl_tree_[n].fc = 0;
    dyn_ltree_[end_block].fc = 1;
    opt_len_ = 0L;
    static_len_ = 0L;
    sym_next_ = 0;
    matches_ = 0;
}

/*  Restore the heap property by moving down the tree starting at node k,
    exchanging a node with the smallest of its two sons if necessary,
    stopping when the heap property is re-established (each father smaller
    than its two sons).
*/
void
deflate_stream::
pqdownheap(
    ct_data const* tree,    // the tree to restore
    int k)                          // node to move down
{
    int v = heap_[k];
    int j = k << 1;  // left son of k
    while(j <= heap_len_)
    {
        // Set j to the smallest of the two sons:
        if(j < heap_len_ &&
                smaller(tree, heap_[j+1], heap_[j]))
            j++;
        // Exit if v is smaller than both sons
        if(smaller(tree, v, heap_[j]))
            break;

        // Exchange v with the smallest son
        heap_[k] = heap_[j];
        k = j;

        // And continue down the tree,
        // setting j to the left son of k
        j <<= 1;
    }
    heap_[k] = v;
}

/*  Remove the smallest element from the heap and recreate the heap
    with one less element. Updates heap and heap_len.
*/
void
deflate_stream::
pqremove(ct_data const* tree, int& top)
{
    top = heap_[kSmallest];
    heap_[kSmallest] = heap_[heap_len_--];
    pqdownheap(tree, kSmallest);
}

/*  Compute the optimal bit lengths for a tree and update the total bit length
    for the current block.
    IN assertion: the fields freq and dad are set, heap[heap_max] and
       above are the tree nodes sorted by increasing frequency.
    OUT assertions: the field len is set to the optimal bit length, the
        array bl_count contains the frequencies for each bit length.
        The length opt_len is updated; static_len is also updated if stree is
        not null.
*/
void
deflate_stream::
gen_bitlen(tree_desc *desc)
{
    ct_data *tree           = desc->dyn_tree;
    int max_code                    = desc->max_code;
    ct_data const* stree    = desc->stat_desc->static_tree;
    std::uint8_t const *extra       = desc->stat_desc->extra_bits;
    int base                        = desc->stat_desc->extra_base;
    int max_length                  = desc->stat_desc->max_length;
    int h;                          // heap index
    int n, m;                       // iterate over the tree elements
    int bits;                       // bit length
    int xbits;                      // extra bits
    std::uint16_t f;                // frequency
    int overflow = 0;               // number of elements with bit length too large

    std::fill(&bl_count_[0], &bl_count_[maxBits+1], std::uint16_t{0});

    /* In a first pass, compute the optimal bit lengths (which may
     * overflow in the case of the bit length tree).
     */
    tree[heap_[heap_max_]].dl = 0; // root of the heap

    for(h = heap_max_+1; h < heap_size; h++) {
        n = heap_[h];
        bits = tree[tree[n].dl].dl + 1;
        if(bits > max_length) bits = max_length, overflow++;
        // We overwrite tree[n].dl which is no longer needed
        tree[n].dl = (std::uint16_t)bits;

        if(n > max_code)
            continue; // not a leaf node

        bl_count_[bits]++;
        xbits = 0;
        if(n >= base)
            xbits = extra[n-base];
        f = tree[n].fc;
        opt_len_ += (std::uint32_t)f * (bits + xbits);
        if(stree)
            static_len_ += (std::uint32_t)f * (stree[n].dl + xbits);
    }
    if(overflow == 0)
        return;

    // Find the first bit length which could increase:
    do
    {
        bits = max_length-1;
        while(bl_count_[bits] == 0)
            bits--;
        bl_count_[bits]--;      // move one leaf down the tree
        bl_count_[bits+1] += 2; // move one overflow item as its brother
        bl_count_[max_length]--;
        /* The brother of the overflow item also moves one step up,
         * but this does not affect bl_count[max_length]
         */
        overflow -= 2;
    }
    while(overflow > 0);

    /* Now recompute all bit lengths, scanning in increasing frequency.
     * h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
     * lengths instead of fixing only the wrong ones. This idea is taken
     * from 'ar' written by Haruhiko Okumura.)
     */
    for(bits = max_length; bits != 0; bits--)
    {
        n = bl_count_[bits];
        while(n != 0)
        {
            m = heap_[--h];
            if(m > max_code)
                continue;
            if((unsigned) tree[m].dl != (unsigned) bits)
            {
                opt_len_ += ((std::uint32_t)bits - tree[m].dl) * tree[m].fc;
                tree[m].dl = (std::uint16_t)bits;
            }
            n--;
        }
    }
}

/*  Construct one Huffman tree and assigns the code bit strings and lengths.
    Update the total bit length for the current block.
    IN assertion: the field freq is set for all tree elements.
    OUT assertions: the fields len and code are set to the optimal bit length
        and corresponding code. The length opt_len is updated; static_len is
        also updated if stree is not null. The field max_code is set.
*/
void
deflate_stream::
build_tree(tree_desc *desc)
{
    ct_data *tree         = desc->dyn_tree;
    ct_data const* stree  = desc->stat_desc->static_tree;
    int elems                     = desc->stat_desc->elems;
    int n, m;          // iterate over heap elements
    int max_code = -1; // largest code with non zero frequency
    int node;          // new node being created

    /* Construct the initial heap, with least frequent element in
     * heap[kSmallest]. The sons of heap[n] are heap[2*n] and heap[2*n+1].
     * heap[0] is not used.
     */
    heap_len_ = 0;
    heap_max_ = heap_size;

    for(n = 0; n < elems; n++)
    {
        if(tree[n].fc != 0)
        {
            heap_[++(heap_len_)] = max_code = n;
            depth_[n] = 0;
        }
        else
        {
            tree[n].dl = 0;
        }
    }

    /* The pkzip format requires that at least one distance code exists,
     * and that at least one bit should be sent even if there is only one
     * possible code. So to avoid special checks later on we force at least
     * two codes of non zero frequency.
     */
    while(heap_len_ < 2)
    {
        node = heap_[++(heap_len_)] = (max_code < 2 ? ++max_code : 0);
        tree[node].fc = 1;
        depth_[node] = 0;
        opt_len_--;
        if(stree)
            static_len_ -= stree[node].dl;
        // node is 0 or 1 so it does not have extra bits
    }
    desc->max_code = max_code;

    /* The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree,
     * establish sub-heaps of increasing lengths:
     */
    for(n = heap_len_/2; n >= 1; n--)
        pqdownheap(tree, n);

    /* Construct the Huffman tree by repeatedly combining the least two
     * frequent nodes.
     */
    node = elems;              /* next internal node of the tree */
    do
    {
        pqremove(tree, n);  /* n = node of least frequency */
        m = heap_[kSmallest]; /* m = node of next least frequency */

        heap_[--(heap_max_)] = n; /* keep the nodes sorted by frequency */
        heap_[--(heap_max_)] = m;

        /* Create a new node father of n and m */
        tree[node].fc = tree[n].fc + tree[m].fc;
        depth_[node] = (std::uint8_t)((depth_[n] >= depth_[m] ?
                                depth_[n] : depth_[m]) + 1);
        tree[n].dl = tree[m].dl = (std::uint16_t)node;
        /* and insert the new node in the heap */
        heap_[kSmallest] = node++;
        pqdownheap(tree, kSmallest);

    }
    while(heap_len_ >= 2);

    heap_[--(heap_max_)] = heap_[kSmallest];

    /* At this point, the fields freq and dad are set. We can now
     * generate the bit lengths.
     */
    gen_bitlen((tree_desc *)desc);

    /* The field len is now set, we can generate the bit codes */
    gen_codes(tree, max_code, bl_count_);
}

/*  Scan a literal or distance tree to determine the frequencies
    of the codes in the bit length tree.
*/
void
deflate_stream::
scan_tree(
    ct_data *tree,      // the tree to be scanned
    int max_code)               // and its largest code of non zero frequency
{
    int n;                      // iterates over all tree elements
    int prevlen = -1;           // last emitted length
    int curlen;                 // length of current code
    int nextlen = tree[0].dl;   // length of next code
    std::uint16_t count = 0;    // repeat count of the current code
    int max_count = 7;          // max repeat count
    int min_count = 4;          // min repeat count

    if(nextlen == 0)
    {
        max_count = 138;
        min_count = 3;
    }
    tree[max_code+1].dl = (std::uint16_t)0xffff; // guard

    for(n = 0; n <= max_code; n++)
    {
        curlen = nextlen; nextlen = tree[n+1].dl;
        if(++count < max_count && curlen == nextlen)
        {
            continue;
        }
        else if(count < min_count)
        {
            bl_tree_[curlen].fc += count;
        }
        else if(curlen != 0)
        {
            if(curlen != prevlen) bl_tree_[curlen].fc++;
            bl_tree_[rep_3_6].fc++;
        }
        else if(count <= 10)
        {
            bl_tree_[repz_3_10].fc++;
        }
        else
        {
            bl_tree_[repz_11_138].fc++;
        }
        count = 0;
        prevlen = curlen;
        if(nextlen == 0)
        {
            max_count = 138;
            min_count = 3;
        }
        else if(curlen == nextlen)
        {
            max_count = 6;
            min_count = 3;
        }
        else
        {
            max_count = 7;
            min_count = 4;
        }
    }
}

/*  Send a literal or distance tree in compressed form,
    using the codes in bl_tree.
*/
void
deflate_stream::
send_tree(
    ct_data *tree,      // the tree to be scanned
    int max_code)               // and its largest code of non zero frequency
{
    int n;                      // iterates over all tree elements
    int prevlen = -1;           // last emitted length
    int curlen;                 // length of current code
    int nextlen = tree[0].dl;   // length of next code
    int count = 0;              // repeat count of the current code
    int max_count = 7;          // max repeat count
    int min_count = 4;          // min repeat count

    // tree[max_code+1].dl = -1; // guard already set
    if(nextlen == 0)
    {
        max_count = 138;
        min_count = 3;
    }

    for(n = 0; n <= max_code; n++)
    {
        curlen = nextlen;
        nextlen = tree[n+1].dl;
        if(++count < max_count && curlen == nextlen)
        {
            continue;
        }
        else if(count < min_count)
        {
            do
            {
                send_code(curlen, bl_tree_);
            }
            while (--count != 0);
        }
        else if(curlen != 0)
        {
            if(curlen != prevlen)
            {
                send_code(curlen, bl_tree_);
                count--;
            }
            BOOST_ASSERT(count >= 3 && count <= 6);
            send_code(rep_3_6, bl_tree_);
            send_bits(count-3, 2);
        }
        else if(count <= 10)
        {
            send_code(repz_3_10, bl_tree_);
            send_bits(count-3, 3);
        }
        else
        {
            send_code(repz_11_138, bl_tree_);
            send_bits(count-11, 7);
        }
        count = 0;
        prevlen = curlen;
        if(nextlen == 0)
        {
            max_count = 138;
            min_count = 3;
        }
        else if(curlen == nextlen)
        {
            max_count = 6;
            min_count = 3;
        }
        else
        {
            max_count = 7;
            min_count = 4;
        }
    }
}

/*  Construct the Huffman tree for the bit lengths and return
    the index in bl_order of the last bit length code to send.
*/
int
deflate_stream::
build_bl_tree()
{
    int max_blindex;  // index of last bit length code of non zero freq

    // Determine the bit length frequencies for literal and distance trees
    scan_tree((ct_data *)dyn_ltree_, l_desc_.max_code);
    scan_tree((ct_data *)dyn_dtree_, d_desc_.max_code);

    // Build the bit length tree:
    build_tree((tree_desc *)(&(bl_desc_)));
    /* opt_len now includes the length of the tree representations, except
     * the lengths of the bit lengths codes and the 5+5+4 bits for the counts.
     */

    /* Determine the number of bit length codes to send. The pkzip format
     * requires that at least 4 bit length codes be sent. (appnote.txt says
     * 3 but the actual value used is 4.)
     */
    for(max_blindex = blCodes-1; max_blindex >= 3; max_blindex--)
    {
        if(bl_tree_[lut_.bl_order[max_blindex]].dl != 0)
            break;
    }
    // Update opt_len to include the bit length tree and counts
    opt_len_ += 3*(max_blindex+1) + 5+5+4;
    return max_blindex;
}

/*  Send the header for a block using dynamic Huffman trees: the counts,
    the lengths of the bit length codes, the literal tree and the distance
    tree.
    IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
*/
void
deflate_stream::
send_all_trees(
    int lcodes,
    int dcodes,
    int blcodes)    // number of codes for each tree
{
    int rank;       // index in bl_order

    BOOST_ASSERT(lcodes >= 257 && dcodes >= 1 && blcodes >= 4);
    BOOST_ASSERT(lcodes <= lCodes && dcodes <= dCodes && blcodes <= blCodes);
    send_bits(lcodes-257, 5); // not +255 as stated in appnote.txt
    send_bits(dcodes-1,   5);
    send_bits(blcodes-4,  4); // not -3 as stated in appnote.txt
    for(rank = 0; rank < blcodes; rank++)
        send_bits(bl_tree_[lut_.bl_order[rank]].dl, 3);
    send_tree((ct_data *)dyn_ltree_, lcodes-1); // literal tree
    send_tree((ct_data *)dyn_dtree_, dcodes-1); // distance tree
}

/*  Send the block data compressed using the given Huffman trees
*/
void
deflate_stream::
compress_block(
    ct_data const* ltree, // literal tree
    ct_data const* dtree) // distance tree
{
    unsigned dist;      /* distance of matched string */
    int lc;             /* match length or unmatched char (if dist == 0) */
    unsigned sx = 0;    /* running index in sym_buf */
    unsigned code;      /* the code to send */
    int extra;          /* number of extra bits to send */

    if(sym_next_ != 0)
    {
        do
        {
            dist = sym_buf_[sx++] & 0xff;
            dist += (unsigned)(sym_buf_[sx++] & 0xff) << 8;
            lc = sym_buf_[sx++];
            if(dist == 0)
            {
                send_code(lc, ltree); /* send a literal byte */
            }
            else
            {
                /* Here, lc is the match length - minMatch */
                code = lut_.length_code[lc];
                send_code(code+literals+1, ltree); /* send the length code */
                extra = lut_.extra_lbits[code];
                if(extra != 0)
                {
                    lc -= lut_.base_length[code];
                    send_bits(lc, extra);       /* send the extra length bits */
                }
                dist--; /* dist is now the match distance - 1 */
                code = d_code(dist);
                BOOST_ASSERT(code < dCodes);

                send_code(code, dtree);       /* send the distance code */
                extra = lut_.extra_dbits[code];
                if(extra != 0)
                {
                    dist -= lut_.base_dist[code];
                    send_bits(dist, extra);   /* send the extra distance bits */
                }
            } /* literal or match pair ? */

            /* Check that the overlay between pending_buf and d_buf+l_buf is ok: */
            BOOST_ASSERT((uInt)(pending_) < lit_bufsize_ + sx);
        }
        while(sx < sym_next_);
    }

    send_code(end_block, ltree);
}

/*  Check if the data type is TEXT or BINARY, using the following algorithm:
    - TEXT if the two conditions below are satisfied:
        a) There are no non-portable control characters belonging to the
            "block list" (0..6, 14..25, 28..31).
        b) There is at least one printable character belonging to the
            "allow list" (9 {TAB}, 10 {LF}, 13 {CR}, 32..255).
    - BINARY otherwise.
    - The following partially-portable control characters form a
        "gray list" that is ignored in this detection algorithm:
        (7 {BEL}, 8 {BS}, 11 {VT}, 12 {FF}, 26 {SUB}, 27 {ESC}).
    IN assertion: the fields fc of dyn_ltree are set.
*/
int
deflate_stream::
detect_data_type()
{
    /* block_mask is the bit mask of block-listed bytes
     * set bits 0..6, 14..25, and 28..31
     * 0xf3ffc07f = binary 11110011111111111100000001111111
     */
    unsigned long block_mask = 0xf3ffc07fUL;
    int n;

    // Check for non-textual ("block-listed") bytes.
    for(n = 0; n <= 31; n++, block_mask >>= 1)
        if((block_mask & 1) && (dyn_ltree_[n].fc != 0))
            return binary;

    // Check for textual ("allow-listed") bytes. */
    if(dyn_ltree_[9].fc != 0 || dyn_ltree_[10].fc != 0
            || dyn_ltree_[13].fc != 0)
        return text;
    for(n = 32; n < literals; n++)
        if(dyn_ltree_[n].fc != 0)
            return text;

    /* There are no "block-listed" or "allow-listed" bytes:
     * this stream either is empty or has tolerated ("gray-listed") bytes only.
     */
    return binary;
}

/*  Flush the bit buffer and align the output on a byte boundary
*/
void
deflate_stream::
bi_windup()
{
    if(bi_valid_ > 8)
        put_short(bi_buf_);
    else if(bi_valid_ > 0)
        put_byte((Byte)bi_buf_);
    bi_buf_ = 0;
    bi_valid_ = 0;
}

/*  Flush the bit buffer, keeping at most 7 bits in it.
*/
void
deflate_stream::
bi_flush()
{
    if(bi_valid_ == 16)
    {
        put_short(bi_buf_);
        bi_buf_ = 0;
        bi_valid_ = 0;
    }
    else if(bi_valid_ >= 8)
    {
        put_byte((Byte)bi_buf_);
        bi_buf_ >>= 8;
        bi_valid_ -= 8;
    }
}

//------------------------------------------------------------------------------

/* Initialize the tree data structures for a new zlib stream.
*/
void
deflate_stream::
tr_init()
{
    l_desc_.dyn_tree = dyn_ltree_;
    l_desc_.stat_desc = &lut_.l_desc;

    d_desc_.dyn_tree = dyn_dtree_;
    d_desc_.stat_desc = &lut_.d_desc;

    bl_desc_.dyn_tree = bl_tree_;
    bl_desc_.stat_desc = &lut_.bl_desc;

    bi_buf_ = 0;
    bi_valid_ = 0;

    // Initialize the first block of the first file:
    init_block();
}

/*  Send one empty static block to give enough lookahead for inflate.
    This takes 10 bits, of which 7 may remain in the bit buffer.
*/
void
deflate_stream::
tr_align()
{
    send_bits(static_trees<<1, 3);
    send_code(end_block, lut_.ltree);
    bi_flush();
}

/* Flush the bits in the bit buffer to pending output (leaves at most 7 bits)
*/
void
deflate_stream::
tr_flush_bits()
{
    bi_flush();
}

/* Send a stored block
*/
void
deflate_stream::
tr_stored_block(
    char *buf,                  // input block
    std::uint32_t stored_len,   // length of input block
    int last)                   // one if this is the last block for a file
{
    send_bits((stored_block<<1)+last, 3);       // send block type
    bi_windup();                                // align on byte boundary
    put_short(static_cast<std::uint16_t>(stored_len));
    put_short(static_cast<std::uint16_t>(~stored_len));
    if(stored_len)
        std::memcpy(&pending_buf_[pending_], buf, stored_len);
    pending_ += stored_len;
}

void
deflate_stream::
tr_tally_dist(std::uint16_t dist, std::uint8_t len, bool& flush)
{
    sym_buf_[sym_next_++] = dist & 0xFF;
    sym_buf_[sym_next_++] = dist >> 8;
    sym_buf_[sym_next_++] = len;
    dist--;
    dyn_ltree_[lut_.length_code[len]+literals+1].fc++;
    dyn_dtree_[d_code(dist)].fc++;
    flush = (sym_next_ == sym_end_);
}

void
deflate_stream::
tr_tally_lit(std::uint8_t c, bool& flush)
{
    sym_buf_[sym_next_++] = 0;
    sym_buf_[sym_next_++] = 0;
    sym_buf_[sym_next_++] = c;
    dyn_ltree_[c].fc++;
    flush = (sym_next_ == sym_end_);
}

//------------------------------------------------------------------------------

/*  Determine the best encoding for the current block: dynamic trees,
    static trees or store, and output the encoded block to the zip file.
*/
void
deflate_stream::
tr_flush_block(
    z_params& zs,
    char *buf,                  // input block, or NULL if too old
    std::uint32_t stored_len,   // length of input block
    int last)                   // one if this is the last block for a file
{
    std::uint32_t opt_lenb;
    std::uint32_t static_lenb;  // opt_len and static_len in bytes
    int max_blindex = 0;        // index of last bit length code of non zero freq

    // Build the Huffman trees unless a stored block is forced
    if(level_ > 0)
    {
        // Check if the file is binary or text
        if(zs.data_type == unknown)
            zs.data_type = detect_data_type();

        // Construct the literal and distance trees
        build_tree((tree_desc *)(&(l_desc_)));

        build_tree((tree_desc *)(&(d_desc_)));
        /* At this point, opt_len and static_len are the total bit lengths of
         * the compressed block data, excluding the tree representations.
         */

        /* Build the bit length tree for the above two trees, and get the index
         * in bl_order of the last bit length code to send.
         */
        max_blindex = build_bl_tree();

        /* Determine the best encoding. Compute the block lengths in bytes. */
        opt_lenb = (opt_len_+3+7)>>3;
        static_lenb = (static_len_+3+7)>>3;

        // zlib's FORCE_STATIC build option is not supported
    #if 0
        opt_lenb = static_lenb;
    #else
        if(static_lenb <= opt_lenb || strategy_ == Strategy::fixed)
            opt_lenb = static_lenb;
    #endif
    }
    else
    {
        BOOST_ASSERT(buf);
        opt_lenb = static_lenb = stored_len + 5; // force a stored block
    }

    // zlib's FORCE_STORED build option is not supported
#if 0
    if(buf != (char*)0) { /* force stored block */
#else
    if(stored_len+4 <= opt_lenb && buf != (char*)0) {
                       /* 4: two words for the lengths */
#endif
        /* The test buf != NULL is only necessary if LIT_BUFSIZE > WSIZE.
         * Otherwise we can't have processed more than WSIZE input bytes since
         * the last block flush, because compression would have been
         * successful. If LIT_BUFSIZE <= WSIZE, it is never too late to
         * transform a block into a stored block.
         */
        tr_stored_block(buf, stored_len, last);
    }
    else if(static_lenb == opt_lenb)
    {
        send_bits((static_trees<<1)+last, 3);
        compress_block(lut_.ltree, lut_.dtree);
    }
    else
    {
        send_bits((dyn_trees<<1)+last, 3);
        send_all_trees(l_desc_.max_code+1, d_desc_.max_code+1,
                       max_blindex+1);
        compress_block((const ct_data *)dyn_ltree_,
                       (const ct_data *)dyn_dtree_);
    }
    /* The above check is made mod 2^32, for files larger than 512 MB
     * and std::size_t implemented on 32 bits.
     */
    init_block();

    if(last)
        bi_windup();
}

/*  Slide the hash table when sliding the window down (could be avoided with 32
    bit values at the expense of memory usage). We slide even when level == 0 to
    keep the hash table consistent if we switch back to level > 0 later.
*/
void
deflate_stream::
slide_hash()
{
    unsigned n, m;
    std::uint16_t *p;
    uInt wsize = w_size_;

    n = hash_size_;
    p = &head_[n];
    do
    {
        m = *--p;
        *p = (std::uint16_t)(m >= wsize ? m-wsize : 0);
    }
    while(--n);

    n = wsize;
    p = &prev_[n];
    do
    {
        m = *--p;
        *p = (std::uint16_t)(m >= wsize ? m-wsize : 0);
        /*  If n is not on any hash chain, prev[n] is garbage but
            its value will never be used.
        */
    }
    while(--n);
}

/*  Fill the window when the lookahead becomes insufficient.
    Updates strstart and lookahead.

    IN assertion: lookahead < kMinLookahead
    OUT assertions: strstart <= window_size-kMinLookahead
       At least one byte has been read, or avail_in == 0; reads are
       performed for at least two bytes (required for the zip translate_eol
       option -- not supported here).
*/
void
deflate_stream::
fill_window(z_params& zs)
{
    unsigned n;
    unsigned more;    // Amount of free space at the end of the window.
    uInt wsize = w_size_;

    BOOST_ASSERT(lookahead_ < kMinLookahead);

    do
    {
        more = (unsigned)(window_size_ -
            (std::uint32_t)lookahead_ -(std::uint32_t)strstart_);

        // VFALCO We don't support systems below 32-bit
    #if 0
        // Deal with !@#$% 64K limit:
        if(sizeof(int) <= 2)
        {
            if(more == 0 && strstart_ == 0 && lookahead_ == 0)
            {
                more = wsize;
            }
            else if(more == (unsigned)(-1))
            {
                /* Very unlikely, but possible on 16 bit machine if
                 * strstart == 0 && lookahead == 1 (input done a byte at time)
                 */
                more--;
            }
        }
    #endif

        /*  If the window is almost full and there is insufficient lookahead,
            move the upper half to the lower one to make room in the upper half.
        */
        if(strstart_ >= wsize+max_dist())
        {
            std::memcpy(window_, window_+wsize, (unsigned)wsize - more);
            match_start_ -= wsize;
            strstart_    -= wsize; // we now have strstart >= max_dist
            block_start_ -= (long) wsize;
            if (insert_ > strstart_)
                insert_ = strstart_;
            slide_hash();
            more += wsize;
        }
        if(zs.avail_in == 0)
            break;

        /*  If there was no sliding:
               strstart <= WSIZE+max_dist-1 && lookahead <= kMinLookahead - 1 &&
               more == window_size - lookahead - strstart
            => more >= window_size - (kMinLookahead-1 + WSIZE + max_dist-1)
            => more >= window_size - 2*WSIZE + 2
            In the BIG_MEM or MMAP case (not yet supported),
              window_size == input_size + kMinLookahead  &&
              strstart + lookahead_ <= input_size => more >= kMinLookahead.
            Otherwise, window_size == 2*WSIZE so more >= 2.
            If there was sliding, more >= WSIZE. So in all cases, more >= 2.
        */
        BOOST_ASSERT(more >= 2);

        n = read_buf(zs, window_ + strstart_ + lookahead_, more);
        lookahead_ += n;

        // Initialize the hash value now that we have some input:
        if(lookahead_ + insert_ >= minMatch)
        {
            uInt str = strstart_ - insert_;
            ins_h_ = window_[str];
            update_hash(ins_h_, window_[str + 1]);
            while(insert_)
            {
                update_hash(ins_h_, window_[str + minMatch-1]);
                prev_[str & w_mask_] = head_[ins_h_];
                head_[ins_h_] = (std::uint16_t)str;
                str++;
                insert_--;
                if(lookahead_ + insert_ < minMatch)
                    break;
            }
        }
        /*  If the whole input has less than minMatch bytes, ins_h is garbage,
            but this is not important since only literal bytes will be emitted.
        */
    }
    while(lookahead_ < kMinLookahead && zs.avail_in != 0);

    /*  If the kWinInit bytes after the end of the current data have never been
        written, then zero those bytes in order to avoid memory check reports of
        the use of uninitialized (or uninitialised as Julian writes) bytes by
        the longest match routines.  Update the high water mark for the next
        time through here.  kWinInit is set to maxMatch since the longest match
        routines allow scanning to strstart + maxMatch, ignoring lookahead.
    */
    if(high_water_ < window_size_)
    {
        std::uint32_t curr = strstart_ + (std::uint32_t)(lookahead_);
        std::uint32_t winit;

        if(high_water_ < curr)
        {
            /*  Previous high water mark below current data -- zero kWinInit
                bytes or up to end of window, whichever is less.
            */
            winit = window_size_ - curr;
            if(winit > kWinInit)
                winit = kWinInit;
            std::memset(window_ + curr, 0, (unsigned)winit);
            high_water_ = curr + winit;
        }
        else if(high_water_ < (std::uint32_t)curr + kWinInit)
        {
            /*  High water mark at or above current data, but below current data
                plus kWinInit -- zero out to current data plus kWinInit, or up
                to end of window, whichever is less.
            */
            winit = (std::uint32_t)curr + kWinInit - high_water_;
            if(winit > window_size_ - high_water_)
                winit = window_size_ - high_water_;
            std::memset(window_ + high_water_, 0, (unsigned)winit);
            high_water_ += winit;
        }
    }

    BOOST_ASSERT((std::uint32_t)strstart_ <= window_size_ - kMinLookahead);
}

/*  Flush as much pending output as possible. All write() output goes
    through this function so some applications may wish to modify it
    to avoid allocating a large strm->next_out buffer and copying into it.
    (See also read_buf()).
*/
void
deflate_stream::
flush_pending(z_params& zs)
{
    tr_flush_bits();
    auto len = clamp(pending_, zs.avail_out);
    if(len == 0)
        return;

    std::memcpy(zs.next_out, pending_out_, len);
    zs.next_out =
        static_cast<std::uint8_t*>(zs.next_out) + len;
    pending_out_  += len;
    zs.total_out += len;
    zs.avail_out  -= len;
    pending_ -= len;
    if(pending_ == 0)
        pending_out_ = pending_buf_;
}

/*  Flush the current block, with given end-of-file flag.
    IN assertion: strstart is set to the end of the current match.
*/
void
deflate_stream::
flush_block(z_params& zs, bool last)
{
    tr_flush_block(zs,
        (block_start_ >= 0L ?
            (char *)&window_[(unsigned)block_start_] :
            (char *)0),
        (std::uint32_t)((long)strstart_ - block_start_),
        last);
   block_start_ = strstart_;
   flush_pending(zs);
}

/*  Read a new buffer from the current input stream, update the adler32
    and total number of bytes read.  All write() input goes through
    this function so some applications may wish to modify it to avoid
    allocating a large strm->next_in buffer and copying from it.
    (See also flush_pending()).
*/
int
deflate_stream::
read_buf(z_params& zs, Byte *buf, unsigned size)
{
    auto len = clamp(zs.avail_in, size);
    if(len == 0)
        return 0;

    zs.avail_in  -= len;

    std::memcpy(buf, zs.next_in, len);
    zs.next_in = static_cast<
        std::uint8_t const*>(zs.next_in) + len;
    zs.total_in += len;
    return (int)len;
}

/*  Set match_start to the longest match starting at the given string and
    return its length. Matches shorter or equal to prev_length are discarded,
    in which case the result is equal to prev_length and match_start is
    garbage.
    IN assertions: cur_match is the head of the hash chain for the current
        string (strstart) and its distance is <= max_dist, and prev_length >= 1
    OUT assertion: the match length is not greater than s->lookahead_.

    For 80x86 and 680x0, an optimized version will be provided in match.asm or
    match.S. The code will be functionally equivalent.
*/
uInt
deflate_stream::
longest_match(IPos cur_match)
{
    unsigned chain_length = max_chain_length_;/* max hash chain length */
    Byte *scan = window_ + strstart_; /* current string */
    Byte *match;                       /* matched string */
    int len;                           /* length of current match */
    int best_len = prev_length_;              /* best match length so far */
    int nice_match = nice_match_;             /* stop if match long enough */
    IPos limit = strstart_ > (IPos)max_dist() ?
        strstart_ - (IPos)max_dist() : 0;
    /* Stop when cur_match becomes <= limit. To simplify the code,
     * we prevent matches with the string of window index 0.
     */
    std::uint16_t *prev = prev_;
    uInt wmask = w_mask_;

    Byte *strend = window_ + strstart_ + maxMatch;
    Byte scan_end1  = scan[best_len-1];
    Byte scan_end   = scan[best_len];

    /* The code is optimized for HASH_BITS >= 8 and maxMatch-2 multiple of 16.
     * It is easy to get rid of this optimization if necessary.
     */
    BOOST_ASSERT(hash_bits_ >= 8 && maxMatch == 258);

    /* Do not waste too much time if we already have a good match: */
    if(prev_length_ >= good_match_) {
        chain_length >>= 2;
    }
    /* Do not look for matches beyond the end of the input. This is necessary
     * to make deflate deterministic.
     */
    if((uInt)nice_match > lookahead_)
        nice_match = lookahead_;

    BOOST_ASSERT((std::uint32_t)strstart_ <= window_size_-kMinLookahead);

    do {
        BOOST_ASSERT(cur_match < strstart_);
        match = window_ + cur_match;

        /* Skip to next match if the match length cannot increase
         * or if the match length is less than 2.  Note that the checks below
         * for insufficient lookahead only occur occasionally for performance
         * reasons.  Therefore uninitialized memory will be accessed, and
         * conditional jumps will be made that depend on those values.
         * However the length of the match is limited to the lookahead, so
         * the output of deflate is not affected by the uninitialized values.
         */
        if(     match[best_len]   != scan_end  ||
                match[best_len-1] != scan_end1 ||
                *match            != *scan     ||
                *++match          != scan[1])
            continue;

        /* The check at best_len-1 can be removed because it will be made
         * again later. (This heuristic is not always a win.)
         * It is not necessary to compare scan[2] and match[2] since they
         * are always equal when the other bytes match, given that
         * the hash keys are equal and that HASH_BITS >= 8.
         */
        scan += 2, match++;
        BOOST_ASSERT(*scan == *match);

        /* We check for insufficient lookahead only every 8th comparison;
         * the 256th check will be made at strstart+258.
         */
        do
        {
        }
        while(  *++scan == *++match && *++scan == *++match &&
                *++scan == *++match && *++scan == *++match &&
                *++scan == *++match && *++scan == *++match &&
                *++scan == *++match && *++scan == *++match &&
                scan < strend);

        BOOST_ASSERT(scan <= window_+(unsigned)(window_size_-1));

        len = maxMatch - (int)(strend - scan);
        scan = strend - maxMatch;

        if(len > best_len) {
            match_start_ = cur_match;
            best_len = len;
            if(len >= nice_match) break;
            scan_end1  = scan[best_len-1];
            scan_end   = scan[best_len];
        }
    }
    while((cur_match = prev[cur_match & wmask]) > limit
        && --chain_length != 0);

    if((uInt)best_len <= lookahead_)
        return (uInt)best_len;
    return lookahead_;
}

//------------------------------------------------------------------------------

/*  Copy without compression as much as possible from the input stream, return
    the current block state.

    In case params() is used to later switch to a non-zero compression
    level, matches_ (otherwise unused when storing) keeps track of the number
    of hash table slides to perform. If matches_ is 1, then one hash table
    slide will be done when switching. If matches_ is 2, the maximum value
    allowed here, then the hash table will be cleared, since two or more slides
    is the same as a clear.

    f_stored() is written to minimize the number of times an input byte is
    copied. It is most efficient with large input and output buffers, which
    maximizes the opportunities to have a single copy from next_in to next_out.
*/
auto
deflate_stream::
f_stored(z_params& zs, Flush flush) ->
    block_state
{
    /* Smallest worthy block size when not flushing or finishing. By default
     * this is 32K. This can be as small as 507 bytes for memLevel == 1. For
     * large input and output buffers, the stored block size will be larger.
     */
    unsigned min_block = clamp(pending_buf_size_ - 5, w_size_);

    /* Copy as many min_block or larger stored blocks directly to next_out as
     * possible. If flushing, copy the remaining available input to next_out as
     * stored blocks, if there is enough space.
     */
    int last = 0;
    unsigned len, left, have;
    std::size_t used = zs.avail_in;
    do
    {
        /* Set len to the maximum size block that we can copy directly with the
         * available input data and output space. Set left to how much of that
         * would be copied from what's left in the window.
         */
        len = kMaxStored;       /* maximum deflate stored block length */
        have = ((unsigned)bi_valid_ + 42) >> 3;   /* bytes in header */
        if(zs.avail_out < have)          /* need room for header */
            break;
            /* maximum stored block length that will fit in avail_out: */
        std::size_t const room = zs.avail_out - have;
        left = (unsigned)(strstart_ - block_start_);    /* window bytes */
        if(len > left + zs.avail_in)
            len = (unsigned)(left + zs.avail_in);   /* limit len to the input */
        if(len > room)
            len = (unsigned)room;                   /* limit len to the output */

        /* If the stored block would be less than min_block in length, or if
         * unable to copy all of the available input when flushing, then try
         * copying to the window and the pending buffer instead. Also don't
         * write an empty block when flushing -- write() does that.
         */
        if(len < min_block && ((len == 0 && flush != Flush::finish) ||
                                flush == Flush::none ||
                                len != left + zs.avail_in))
            break;

        /* Make a dummy stored block in pending to get the header bytes,
         * including any pending bits.
         */
        last = flush == Flush::finish && len == left + zs.avail_in ? 1 : 0;
        tr_stored_block(nullptr, 0L, last);

        /* Replace the lengths in the dummy stored block with len. */
        pending_buf_[pending_ - 4] = (Byte)len;
        pending_buf_[pending_ - 3] = (Byte)(len >> 8);
        pending_buf_[pending_ - 2] = (Byte)~len;
        pending_buf_[pending_ - 1] = (Byte)(~len >> 8);

        /* Write the stored block header bytes. */
        flush_pending(zs);

        /* Copy uncompressed bytes from the window to next_out. */
        if(left)
        {
            if(left > len)
                left = len;
            std::memcpy(zs.next_out, window_ + block_start_, left);
            zs.next_out = static_cast<std::uint8_t*>(zs.next_out) + left;
            zs.avail_out -= left;
            zs.total_out += left;
            block_start_ += left;
            len -= left;
        }

        /* Copy uncompressed bytes directly from next_in to next_out. */
        if(len)
        {
            read_buf(zs, static_cast<Byte*>(zs.next_out), len);
            zs.next_out = static_cast<std::uint8_t*>(zs.next_out) + len;
            zs.avail_out -= len;
            zs.total_out += len;
        }
    }
    while(last == 0);

    /* Update the sliding window with the last w_size_ bytes of the copied
     * data, or append all of the copied data to the existing window if less
     * than w_size_ bytes were copied. Also update the number of bytes to
     * insert in the hash tables, in the event that params() switches to
     * a non-zero compression level.
     */
    used -= zs.avail_in;      /* number of input bytes directly copied */
    if(used)
    {
        auto const next_in =
            static_cast<std::uint8_t const*>(zs.next_in);
        /* If any input was used, then no unused input remains in the window,
         * therefore block_start_ == strstart_.
         */
        if(used >= w_size_)    /* supplant the previous history */
        {
            matches_ = 2;         /* clear hash */
            std::memcpy(window_, next_in - w_size_, w_size_);
            strstart_ = w_size_;
            insert_ = strstart_;
        }
        else
        {
            if(window_size_ - strstart_ <= used)
            {
                /* Slide the window down. */
                strstart_ -= w_size_;
                std::memcpy(window_, window_ + w_size_, strstart_);
                if(matches_ < 2)
                    matches_++;   /* add a pending slide_hash() */
                if(insert_ > strstart_)
                    insert_ = strstart_;
            }
            std::memcpy(window_ + strstart_, next_in - used, used);
            strstart_ += (uInt)used;
            insert_ += clamp((uInt)used, w_size_ - insert_);
        }
        block_start_ = strstart_;
    }
    if(high_water_ < strstart_)
        high_water_ = strstart_;

    /* If the last block was written to next_out, then done. */
    if(last)
        return finish_done;

    /* If flushing and all input has been consumed, then done. */
    if(flush != Flush::none && flush != Flush::finish &&
        zs.avail_in == 0 && (long)strstart_ == block_start_)
        return block_done;

    /* Fill the window with any remaining input. */
    have = (unsigned)(window_size_ - strstart_);
    if(zs.avail_in > have && block_start_ >= (long)w_size_)
    {
        /* Slide the window down. */
        block_start_ -= w_size_;
        strstart_ -= w_size_;
        std::memcpy(window_, window_ + w_size_, strstart_);
        if(matches_ < 2)
            matches_++;           /* add a pending slide_hash() */
        have += w_size_;          /* more space now */
        if(insert_ > strstart_)
            insert_ = strstart_;
    }
    if(have > zs.avail_in)
        have = (unsigned)zs.avail_in;
    if(have)
    {
        read_buf(zs, window_ + strstart_, have);
        strstart_ += have;
        insert_ += clamp(have, w_size_ - insert_);
    }
    if(high_water_ < strstart_)
        high_water_ = strstart_;

    /* There was not enough avail_out to write a complete worthy or flushed
     * stored block to next_out. Write a stored block to pending instead, if we
     * have enough input for a worthy block, or if flushing and there is enough
     * room for the remaining input as a stored block in the pending buffer.
     */
    have = ((unsigned)bi_valid_ + 42) >> 3;   /* bytes in header */
        /* maximum stored block length that will fit in pending: */
    have = clamp(pending_buf_size_ - have, kMaxStored);
    min_block = clamp(have, w_size_);
    left = (unsigned)(strstart_ - block_start_);
    if(left >= min_block ||
        ((left || flush == Flush::finish) && flush != Flush::none &&
         zs.avail_in == 0 && left <= have))
    {
        len = clamp(left, have);
        last = flush == Flush::finish && zs.avail_in == 0 &&
               len == left ? 1 : 0;
        tr_stored_block((char *)window_ + block_start_, len, last);
        block_start_ += len;
        flush_pending(zs);
    }

    /* We've done all we can with the available input and output. */
    return last ? finish_started : need_more;
}

/*  Compress as much as possible from the input stream, return the current
    block state.
    This function does not perform lazy evaluation of matches and inserts
    new strings in the dictionary only for unmatched strings or for short
    matches. It is used only for the fast compression options.
*/
auto
deflate_stream::
f_fast(z_params& zs, Flush flush) ->
    block_state
{
    IPos hash_head;       /* head of the hash chain */
    bool bflush;           /* set if current block must be flushed */

    for(;;)
    {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need maxMatch bytes
         * for the next match, plus minMatch bytes to insert the
         * string following the next match.
         */
        if(lookahead_ < kMinLookahead)
        {
            fill_window(zs);
            if(lookahead_ < kMinLookahead && flush == Flush::none)
                return need_more;
            if(lookahead_ == 0)
                break; /* flush the current block */
        }

        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        hash_head = 0;
        if(lookahead_ >= minMatch) {
            insert_string(hash_head);
        }

        /* Find the longest match, discarding those <= prev_length.
         * At this point we have always match_length < minMatch
         */
        if(hash_head != 0 && strstart_ - hash_head <= max_dist()) {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
            match_length_ = longest_match (hash_head);
            /* longest_match() sets match_start */
        }
        if(match_length_ >= minMatch)
        {
            tr_tally_dist(static_cast<std::uint16_t>(strstart_ - match_start_),
                static_cast<std::uint8_t>(match_length_ - minMatch), bflush);

            lookahead_ -= match_length_;

            /* Insert new strings in the hash table only if the match length
             * is not too large. This saves time but degrades compression.
             */
            if(match_length_ <= max_lazy_match_ &&
                lookahead_ >= minMatch) {
                match_length_--; /* string at strstart already in table */
                do
                {
                    strstart_++;
                    insert_string(hash_head);
                    /* strstart never exceeds WSIZE-maxMatch, so there are
                     * always minMatch bytes ahead.
                     */
                }
                while(--match_length_ != 0);
                strstart_++;
            }
            else
            {
                strstart_ += match_length_;
                match_length_ = 0;
                ins_h_ = window_[strstart_];
                update_hash(ins_h_, window_[strstart_+1]);
                /* If lookahead < minMatch, ins_h is garbage, but it does not
                 * matter since it will be recomputed at next deflate call.
                 */
            }
        }
        else
        {
            /* No match, output a literal byte */
            tr_tally_lit(window_[strstart_], bflush);
            lookahead_--;
            strstart_++;
        }
        if(bflush)
        {
            flush_block(zs, false);
            if(zs.avail_out == 0)
                return need_more;
        }
    }
    insert_ = strstart_ < minMatch-1 ? strstart_ : minMatch-1;
    if(flush == Flush::finish)
    {
        flush_block(zs, true);
        if(zs.avail_out == 0)
            return finish_started;
        return finish_done;
    }
    if(sym_next_)
    {
        flush_block(zs, false);
        if(zs.avail_out == 0)
            return need_more;
    }
    return block_done;
}

/*  Same as above, but achieves better compression. We use a lazy
    evaluation for matches: a match is finally adopted only if there is
    no better match at the next window position.
*/
auto
deflate_stream::
f_slow(z_params& zs, Flush flush) ->
    block_state
{
    IPos hash_head;          /* head of hash chain */
    bool bflush;              /* set if current block must be flushed */

    /* Process the input block. */
    for(;;)
    {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need maxMatch bytes
         * for the next match, plus minMatch bytes to insert the
         * string following the next match.
         */
        if(lookahead_ < kMinLookahead)
        {
            fill_window(zs);
            if(lookahead_ < kMinLookahead && flush == Flush::none)
                return need_more;
            if(lookahead_ == 0)
                break; /* flush the current block */
        }

        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        hash_head = 0;
        if(lookahead_ >= minMatch)
            insert_string(hash_head);

        /* Find the longest match, discarding those <= prev_length.
         */
        prev_length_ = match_length_, prev_match_ = match_start_;
        match_length_ = minMatch-1;

        if(hash_head != 0 && prev_length_ < max_lazy_match_ &&
            strstart_ - hash_head <= max_dist())
        {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
            match_length_ = longest_match(hash_head);
            /* longest_match() sets match_start */

            if(match_length_ <= 5 && (strategy_ == Strategy::filtered
                || (match_length_ == minMatch &&
                    strstart_ - match_start_ > kTooFar)
                ))
            {
                /* If prev_match is also minMatch, match_start is garbage
                 * but we will ignore the current match anyway.
                 */
                match_length_ = minMatch-1;
            }
        }
        /* If there was a match at the previous step and the current
         * match is not better, output the previous match:
         */
        if(prev_length_ >= minMatch && match_length_ <= prev_length_)
        {
            /* Do not insert strings in hash table beyond this. */
            uInt max_insert = strstart_ + lookahead_ - minMatch;

            tr_tally_dist(
                static_cast<std::uint16_t>(strstart_ -1 - prev_match_),
                static_cast<std::uint8_t>(prev_length_ - minMatch), bflush);

            /* Insert in hash table all strings up to the end of the match.
             * strstart-1 and strstart are already inserted. If there is not
             * enough lookahead, the last two strings are not inserted in
             * the hash table.
             */
            lookahead_ -= prev_length_-1;
            prev_length_ -= 2;
            do {
                if(++strstart_ <= max_insert)
                    insert_string(hash_head);
            }
            while(--prev_length_ != 0);
            match_available_ = 0;
            match_length_ = minMatch-1;
            strstart_++;

            if(bflush)
            {
                flush_block(zs, false);
                if(zs.avail_out == 0)
                    return need_more;
            }

        }
        else if(match_available_)
        {
            /* If there was no match at the previous position, output a
             * single literal. If there was a match but the current match
             * is longer, truncate the previous match to a single literal.
             */
            tr_tally_lit(window_[strstart_-1], bflush);
            if(bflush)
                flush_block(zs, false);
            strstart_++;
            lookahead_--;
            if(zs.avail_out == 0)
                return need_more;
        }
        else
        {
            /* There is no previous match to compare with, wait for
             * the next step to decide.
             */
            match_available_ = 1;
            strstart_++;
            lookahead_--;
        }
    }
    BOOST_ASSERT(flush != Flush::none);
    if(match_available_)
    {
        tr_tally_lit(window_[strstart_-1], bflush);
        match_available_ = 0;
    }
    insert_ = strstart_ < minMatch-1 ? strstart_ : minMatch-1;
    if(flush == Flush::finish)
    {
        flush_block(zs, true);
        if(zs.avail_out == 0)
            return finish_started;
        return finish_done;
    }
    if(sym_next_)
    {
        flush_block(zs, false);
        if(zs.avail_out == 0)
            return need_more;
    }
    return block_done;
}

/*  For Strategy::rle, simply look for runs of bytes, generate matches only of distance
    one.  Do not maintain a hash table.  (It will be regenerated if this run of
    deflate switches away from Strategy::rle.)
*/
auto
deflate_stream::
f_rle(z_params& zs, Flush flush) ->
    block_state
{
    bool bflush;            // set if current block must be flushed
    uInt prev;              // byte at distance one to match
    Byte *scan, *strend;    // scan goes up to strend for length of run

    for(;;)
    {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need maxMatch bytes
         * for the longest run, plus one for the unrolled loop.
         */
        if(lookahead_ <= maxMatch) {
            fill_window(zs);
            if(lookahead_ <= maxMatch && flush == Flush::none) {
                return need_more;
            }
            if(lookahead_ == 0) break; /* flush the current block */
        }

        /* See how many times the previous byte repeats */
        match_length_ = 0;
        if(lookahead_ >= minMatch && strstart_ > 0) {
            scan = window_ + strstart_ - 1;
            prev = *scan;
            if(prev == *++scan && prev == *++scan && prev == *++scan) {
                strend = window_ + strstart_ + maxMatch;
                do {
                } while(prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         scan < strend);
                match_length_ = maxMatch - (uInt)(strend - scan);
                if(match_length_ > lookahead_)
                    match_length_ = lookahead_;
            }
            BOOST_ASSERT(scan <= window_+(uInt)(window_size_-1));
        }

        /* Emit match if have run of minMatch or longer, else emit literal */
        if(match_length_ >= minMatch) {
            tr_tally_dist(std::uint16_t{1},
                static_cast<std::uint8_t>(match_length_ - minMatch),
                bflush);

            lookahead_ -= match_length_;
            strstart_ += match_length_;
            match_length_ = 0;
        } else {
            /* No match, output a literal byte */
            tr_tally_lit(window_[strstart_], bflush);
            lookahead_--;
            strstart_++;
        }
        if(bflush)
        {
            flush_block(zs, false);
            if(zs.avail_out == 0)
                return need_more;
        }
    }
    insert_ = 0;
    if(flush == Flush::finish)
    {
        flush_block(zs, true);
        if(zs.avail_out == 0)
            return finish_started;
        return finish_done;
    }
    if(sym_next_)
    {
        flush_block(zs, false);
        if(zs.avail_out == 0)
            return need_more;
    }
    return block_done;
}

/* ===========================================================================
 * For Strategy::huffman, do not look for matches.  Do not maintain a hash table.
 * (It will be regenerated if this run of deflate switches away from Huffman.)
 */
auto
deflate_stream::
f_huff(z_params& zs, Flush flush) ->
    block_state
{
    bool bflush;             // set if current block must be flushed

    for(;;)
    {
        // Make sure that we have a literal to write.
        if(lookahead_ == 0)
        {
            fill_window(zs);
            if(lookahead_ == 0)
            {
                if(flush == Flush::none)
                    return need_more;
                break;      // flush the current block
            }
        }

        // Output a literal byte
        match_length_ = 0;
        tr_tally_lit(window_[strstart_], bflush);
        lookahead_--;
        strstart_++;
        if(bflush)
        {
            flush_block(zs, false);
            if(zs.avail_out == 0)
                return need_more;
        }
    }
    insert_ = 0;
    if(flush == Flush::finish)
    {
        flush_block(zs, true);
        if(zs.avail_out == 0)
            return finish_started;
        return finish_done;
    }
    if(sym_next_)
    {
        flush_block(zs, false);
        if(zs.avail_out == 0)
            return need_more;
    }
    return block_done;
}

} // detail
} // zlib
} // beast
} // boost

#endif
