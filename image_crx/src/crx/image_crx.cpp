#include <iostream>
#include <cstring>
#include <image_crx.hpp>
#include <zlib.h>

namespace crx
{

    static auto pack_v1(std::span<const uint8_t> pixels, int width, int height, int bpp, std::vector<uint8_t>& output) noexcept -> bool
    {
        const int    row_bytes    { width * bpp };
        const int    padded_stride{ ((row_bytes + 3) / 4) * 4 };
        const size_t src_size     { static_cast<size_t>(padded_stride) * height };

        output.clear();
        output.reserve(src_size / 2);

        constexpr size_t window_size{ 0x10000 };

        uint8_t flag_byte{};
        size_t  flag_pos { static_cast<size_t>(-1) };
        int     flag_count{};

        const auto start_flag_group{ [&]() noexcept -> void
        {
            flag_pos = output.size();
            output.push_back(0);
            flag_count = 0;
        } };

        const auto end_flag_group{ [&]() noexcept -> void
        {
            output[flag_pos] = flag_byte;
            flag_byte  = 0;
            flag_count = 0;
        } };

        const auto emit_literal{ [&](const uint8_t dat) noexcept -> void
        {
            if (flag_count == 0)
            {
                start_flag_group();
            }
            flag_byte |= static_cast<uint8_t>(1 << flag_count);
            output.push_back(dat);
            if (++flag_count == 8)
            {
                end_flag_group();
            }
        } };

        const auto emit_match{ [&](const int offset, const int count) noexcept -> void
        {
            if (flag_count == 0)
            {
                start_flag_group();
            }
            ++flag_count;

            if (count >= 4 && count <= 19 && offset <= 0x3FF)
            {
                output.push_back(static_cast<uint8_t>(0xC0 | ((count - 4) << 2) | ((offset >> 8) & 3)));
                output.push_back(static_cast<uint8_t>(offset & 0xFF));
            }
            else if (count >= 2 && count <= 5 && offset >= 1 && offset <= 0x1F)
            {
                output.push_back(static_cast<uint8_t>(0x80 | ((count - 2) << 5) | offset));
            }
            else if (count >= 4 && count <= 131)
            {
                output.push_back(static_cast<uint8_t>(count - 4));
                output.push_back(static_cast<uint8_t>(offset & 0xFF));
                output.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
            }
            else
            {
                output.push_back(0x7F);
                output.push_back(static_cast<uint8_t>((count - 2) & 0xFF));
                output.push_back(static_cast<uint8_t>(((count - 2) >> 8) & 0xFF));
                output.push_back(static_cast<uint8_t>(offset & 0xFF));
                output.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
            }

            if (flag_count == 8)
            {
                end_flag_group();
            }
        } };

        const auto try_emit_match{ [&](const int offset, const int count) noexcept -> void
        {
            int remaining{ count };
            while (remaining > 0)
            {
                int chunk{ remaining };

                if (offset <= 0x3FF && chunk >= 4)
                {
                    if (chunk > 19) { chunk = 19; }
                }
                else if (offset >= 1 && offset <= 0x1F && chunk >= 2)
                {
                    if (chunk > 5) { chunk = 5; }
                }
                else if (chunk >= 4)
                {
                    if (chunk > 131) { chunk = 131; }
                }

                emit_match(offset, chunk);
                remaining -= chunk;
            }
        } };

        size_t src_pos{};
        while (src_pos < src_size)
        {
            int best_offset{};
            int best_count{};

            const size_t max_len     { src_size - src_pos };
            const size_t search_start{ src_pos > window_size ? src_pos - window_size : 0 };

            for (size_t back{ 1 }; back <= src_pos - search_start; ++back)
            {
                const size_t match_pos{ src_pos - back };
                int len{};

                while (len < static_cast<int>(max_len) && pixels[match_pos + static_cast<size_t>(len)] == pixels[src_pos + static_cast<size_t>(len)])
                {
                    ++len;
                }

                if (len > best_count)
                {
                    best_count  = len;
                    best_offset = static_cast<int>(back);
                    if (best_count >= static_cast<int>(max_len))
                    {
                        break;
                    }
                }
            }

            if (best_count >= 2)
            {
                try_emit_match(best_offset, best_count);
                src_pos += static_cast<size_t>(best_count);
            }
            else
            {
                emit_literal(pixels[src_pos++]);
            }
        }

        if (flag_count > 0)
        {
            output[flag_pos] = flag_byte;
        }

        return true;
    }

    static auto pack_v2(std::span<const uint8_t> pixels, int width, int height, int bpp, std::vector<uint8_t>& output) noexcept -> bool
    {
        const int pixel_size   { bpp };
        const int src_stride   { width * pixel_size };
        const int padded_stride{ ((src_stride + 3) / 4) * 4 };

        const size_t filtered_size{ static_cast<size_t>(src_stride + 1) * height };
        std::vector<uint8_t> filtered{};
        filtered.resize(filtered_size);

        size_t dst_pos{};
        for (int y{}; y < height; ++y)
        {
            const uint8_t* src_row{ pixels.data() + static_cast<size_t>(y) * padded_stride };

            filtered[dst_pos++] = 0;

            for (int x{}; x < pixel_size; ++x)
            {
                filtered[dst_pos++] = src_row[x];
            }
            for (int x{ pixel_size }; x < src_stride; ++x)
            {
                filtered[dst_pos++] = static_cast<uint8_t>(src_row[x] - src_row[x - pixel_size]);
            }
        }

        const uLongf max_len{ ::compressBound(static_cast<uLong>(filtered_size)) };
        output.resize(max_len);

        uLongf dest_len{ max_len };
        const int result{ ::compress2(output.data(), &dest_len, filtered.data(), static_cast<uLong>(filtered_size), Z_BEST_COMPRESSION) };
        if (result != Z_OK)
        {
            return false;
        }

        output.resize(dest_len);
        return true;
    }

    static auto unpack_v1(const crxg_header* header, const std::span<const uint8_t> data, std::vector<uint8_t>& output) noexcept -> bool
    {
        const int   row_bytes{ header->width * header->bpp() };
        const int pixels_size{ ((row_bytes + 3) / 4) * 4 * header->height };

        if (output.capacity() < pixels_size) 
        {
            output.clear();
            output.reserve(pixels_size);
        }
        output.resize(pixels_size);

        std::vector<uint8_t> buffer{};
        buffer.resize(0x10000);

        int flag{};
        size_t buf_pos{}, dst_pos{}, src_pos{};

        const uint8_t* packed{ data.data() };
        size_t    packed_size{ data.size() };

        while (dst_pos < pixels_size)
        {
            flag >>= 1;
            if (0 == (flag & 0x100))
            {
                if (src_pos >= packed_size) 
                { 
                    return false;
                }

                flag = packed[src_pos++] | 0xff00;
            }

            if (0 != (flag & 1))
            {
                if (src_pos >= packed_size) 
                {
                    return false;
                }
                uint8_t dat{ packed[src_pos++] };
                buffer[buf_pos++] = dat;
                buf_pos &= 0xffff;
                output[dst_pos++] = dat;
            }
            else
            {
                if (src_pos >= packed_size)
                {
                    return false;
                }

                uint8_t control{ packed[src_pos++] };
                int count{}, offset{};

                if (control >= 0xc0)
                {
                    if (src_pos + 1 > packed_size)
                    {
                        return false;
                    }

                    offset = ((control & 3) << 8) | packed[src_pos++];
                    count = 4 + ((control >> 2) & 0xf);
                }
                else if (0 != (control & 0x80))
                {
                    offset = control & 0x1f;
                    count = 2 + ((control >> 5) & 3);
                    if (0 == offset)
                    {
                        if (src_pos >= packed_size)
                        {
                            return false;
                        }
                        offset = packed[src_pos++];
                    }
                }
                else if (0x7f == control)
                {
                    if (src_pos + 4 > packed_size)
                    {
                        return false;
                    }

                    count = 2 + (packed[src_pos] | (packed[src_pos + 1] << 8));
                    src_pos += 2;
                    offset = packed[src_pos] | (packed[src_pos + 1] << 8);
                    src_pos += 2;
                }
                else
                {
                    if (src_pos + 2 > packed_size) 
                    {
                        return false;
                    }

                    offset = packed[src_pos] | (packed[src_pos + 1] << 8);
                    src_pos += 2;
                    count = control + 4;
                }

                offset = (static_cast<int>(buf_pos) - offset) & 0xffff;
                for (int k{}; k < count && dst_pos < pixels_size; ++k)
                {
                    uint8_t dat{ buffer[offset++] };
                    offset &= 0xffff;
                    buffer[buf_pos++] = dat;
                    buf_pos &= 0xffff;
                    output[dst_pos++] = dat;
                }
            }
        }
        return true;
    }

    static auto unpack_v2(const crxg_header* header, const std::span<const uint8_t> data, std::vector<uint8_t>& output) noexcept -> bool
    {
        const int   w{ header->width  };
        const int   h{ header->height };
        const int   pixel_size{ header->bpp() };
        const int   row_bytes{ w * pixel_size };
        const int      stride{ ((row_bytes + 3) / 4) * 4 };
        const int pixels_size{ stride * h };

        const uint8_t* packed{ data.data() };
        size_t    packed_size{ data.size() };

        size_t raw_size = (size_t)w * h * pixel_size + 0x2000;
        std::vector<uint8_t> raw(raw_size);
        uLongf dest_len = (uLongf)raw_size;

        const int zret = ::uncompress(raw.data(), &dest_len, packed, (uLong)packed_size);
        if (zret != Z_OK) return false;
        raw.resize(dest_len);

        output.resize((size_t)stride * h);
        std::memset(output.data(), 0, output.size());

        const uint8_t* src = raw.data();
        const uint8_t* src_end = raw.data() + raw.size();

        for (int y = 0; y < h; ++y)
        {
            if (src >= src_end) return false;

            uint8_t* dst = output.data() + y * stride;

            switch (*src++)
            {
            case 0:
            {
                std::memcpy(dst, src, (size_t)pixel_size);
                src += pixel_size;
                for (int x = pixel_size; x < row_bytes; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)dst[x - pixel_size]);
                break;
            }
            case 1:
            {
                uint8_t* prev = dst - stride;
                for (int x = 0; x < row_bytes; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x]);
                break;
            }
            case 2:
            {
                uint8_t* prev = dst - stride;
                std::memcpy(dst, src, (size_t)pixel_size);
                src += pixel_size;
                for (int x = pixel_size; x < row_bytes; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x - pixel_size]);
                break;
            }
            case 3:
            {
                uint8_t* prev = dst - stride;
                int count = row_bytes - pixel_size;
                for (int x = 0; x < count; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x + pixel_size]);
                std::memcpy(dst + count, src, (size_t)pixel_size);
                src += pixel_size;
                break;
            }
            case 4:
            {
                for (int ch = 0; ch < pixel_size; ++ch)
                {
                    int ww = w;
                    uint8_t val = *src++;
                    uint8_t* ptr = dst + ch;
                    while (ww > 0)
                    {
                        *ptr = val;
                        ptr += pixel_size;
                        if (0 == --ww) break;
                        uint8_t next = *src++;
                        if (val == next)
                        {
                            int count = *src++;
                            for (int j = 0; j < count; ++j)
                            {
                                *ptr = val;
                                ptr += pixel_size;
                            }
                            ww -= count;
                            if (ww > 0) val = *src++;
                        }
                        else
                        {
                            val = next;
                        }
                    }
                }
                break;
            }
            default:
                return false;
            }
        }

        return true;
    }

    crxg_view::crxg_view(const std::span<const uint8_t> data) noexcept : m_raw{ data }
    {
        if (this->m_raw.data() == nullptr || this->m_raw.size() < sizeof(crx::crxg_header))
        {
            return;
        }
        
        this->m_header = crx::crxg_header::cast(this->m_raw.data());

        if (!this->m_header->is_valid())
        {
            return;
        }

        size_t crxg_data_offset{};
        const auto crxg_data{ this->m_raw.subspan(sizeof(crx::crxg_header)) };

        if (this->m_header->crx_type >= 3)
        {
            if (crxg_data.size() < 4)
            {
                return;
            }

            this->m_extended = crx::extended_header::cast(crxg_data.data());

            const auto ext_bytes
            {
                this->m_extended->count * sizeof(crx::extended_entry)
            };

            if (crxg_data.size() - crxg_data_offset < ext_bytes)
            {
                return;
            }

            crxg_data_offset += ext_bytes + 4;
        }

        if (this->m_header->flags & 0x10)
        {
            if (crxg_data.size() - crxg_data_offset < 4)
            {
                return;
            }
            crxg_data_offset += 4;
        }

        if (this->m_header->bpp() == 1)
        {

            const int           colors{ this->m_header->colors > 0x100 ? 0x100 : this->m_header->colors };
            const int color_entry_size{ this->m_header->colors == 0x102 ? 4 : 3 };
            
            this->m_palette.size   = color_entry_size;
            this->m_palette.length = colors;

            const auto palette_size{ static_cast<size_t>(colors * color_entry_size) };
            this->m_palette.data   = crxg_data.data() + crxg_data_offset;

            crxg_data_offset += static_cast<size_t>(colors * color_entry_size);
        }

        this->m_compress = crxg_data.subspan(crxg_data_offset);
    }

    auto image_crx::unpack(std::vector<uint8_t>& buffer) const noexcept -> bool
    {
        if (!this->is_valid())
        {
            return false;
        }

        if (this->m_view.m_header->crx_type == 1)
        {
            if (!crx::unpack_v1(this->m_view.m_header, this->m_view.m_compress, buffer))
            {
                return false;
            }
        }
        else
        {
            if (!crx::unpack_v2(this->m_view.m_header, this->m_view.m_compress, buffer))
            {
                return false;
            }
        }

        return true;
    }

    auto image_crx::decode(std::vector<uint8_t>& buffer, pixels::type format, uint8_t keyidx) const noexcept -> bool
    {
        if (!(format >= pixels::RGBA && format <= pixels::BGR))
        {
            return false;
        }

        if (!this->unpack(buffer)) 
        {
            return false;
        }

        bool has_alpha{ true };
        int8_t r_off{}, g_off{}, b_off{}, a_off{};
        switch (format)
        {
            case pixels::RGBA: r_off = 0; g_off = 1; b_off = 2; a_off = 3; break;
            case pixels::BGRA: b_off = 0; g_off = 1; r_off = 2; a_off = 3; break;
            case pixels::ARGB: a_off = 0; r_off = 1; g_off = 2; b_off = 3; break;
            case pixels::ABGR: a_off = 0; b_off = 1; g_off = 2; r_off = 3; break;
            case pixels::RGB:  r_off = 0; g_off = 1; b_off = 2; has_alpha = false; break;
            case pixels::BGR:  b_off = 0; g_off = 1; r_off = 2; has_alpha = false; break;
        }

        std::vector<uint8_t> temp{};
        const auto* header  { this->m_view.m_header };
        const int   w       { header->width  };
        const int   h       { header->height };
        const int   src_bpp { header->bpp()  };
        const int   src_row_bytes { w * src_bpp };
        const int   src_stride    { ((src_row_bytes + 3) / 4) * 4 };

        const int   dst_bpp   { has_alpha ? 4 : 3 };
        const int   dst_stride{ w * dst_bpp };
        const size_t dst_size { static_cast<size_t>(dst_stride) * h };
        if (src_bpp == 1)
        {
            const auto& pal{ this->m_view.m_palette };
            if (pal.data == nullptr || pal.length == 0) 
            {
                return false;
            }

            if (buffer.capacity() < dst_size)
            {
                temp = std::move(buffer);
            }
            buffer.resize(dst_size);

            const uint8_t* src_base{ temp.empty() ? buffer.data() : temp.data() };
            for (int y{ h - 1 }; y >= 0; --y)
            {
                const uint8_t* src_row{ src_base + static_cast<size_t>(y) * src_stride };
                uint8_t*       dst_row{ buffer.data() + static_cast<size_t>(y) * dst_stride };
                for (int x{ w - 1 }; x >= 0; --x)
                {
                    const uint8_t idx{ src_row[x] };
                    const size_t pal_off{ static_cast<size_t>(idx < pal.length ? idx : 0) * pal.size };

                    uint8_t* px{ dst_row + x * dst_bpp };
                    px[r_off] = pal.data[pal_off + 0];
                    px[g_off] = pal.data[pal_off + 1];
                    px[b_off] = pal.data[pal_off + 2];
                    if (has_alpha)
                    {
                        px[a_off] = (idx == keyidx) ? 0 : 255;
                    }
                }
            }
            return true;
        }
        if (src_bpp == 3)
        {
            if (buffer.capacity() < dst_size) 
            {
                temp = std::move(buffer);
            }
            buffer.resize(dst_size);

            const uint8_t* src_base{ temp.empty() ? buffer.data() : temp.data() };

            for (int y{ h - 1 }; y >= 0; --y)
            {
                const uint8_t* src_row{ src_base + static_cast<size_t>(y) * src_stride };
                uint8_t*       dst_row{ buffer.data() + static_cast<size_t>(y) * dst_stride };
                for (int x{ w - 1 }; x >= 0; --x)
                {
                    uint8_t* px{ dst_row + x * dst_bpp };
                    px[r_off] = src_row[x * 3 + 2];
                    px[g_off] = src_row[x * 3 + 1];
                    px[b_off] = src_row[x * 3 + 0];
                    if (has_alpha)
                    {
                        px[a_off] = 255;
                    }
                }
            }
            return true;
        }
        if (src_bpp == 4 && header->mode != 1)
        {
            const int alpha_flip{ header->mode == 2 ? 0 : 0xFF };
            for (int y{}; y < h; ++y)
            {
                const uint8_t* src_row{ buffer.data() + static_cast<size_t>(y) * src_stride };
                uint8_t*       dst_row{ buffer.data() + static_cast<size_t>(y) * dst_stride };
                for (int x{}; x < w; ++x)
                {
                    const uint8_t* p{ src_row + x * 4 }; // [A, B, G, R]
                    const uint8_t  a{ static_cast<uint8_t>(p[0] ^ alpha_flip) };
                    const uint8_t  b{ p[1] }, g{ p[2] }, r{ p[3] };

                    uint8_t* px{ dst_row + x * dst_bpp };
                    px[r_off] = r;
                    px[g_off] = g;
                    px[b_off] = b;
                    if (has_alpha)
                    {
                        px[a_off] = a;
                    }
                }
            }
            buffer.resize(dst_size);
        }
        return true;
    }

    auto image_crx::encode(const crx::pixels& data, std::vector<uint8_t>& buffer, std::span<uint8_t> header) noexcept -> bool
    {
        if (data.data.empty() || data.width <= 0 || data.height <= 0)
        {
            return false;
        }

        const bool has_alpha
        {
            data.format == pixels::rgba || data.format == pixels::bgra ||
            data.format == pixels::argb || data.format == pixels::abgr
        };
        const int channels{ has_alpha ? 4 : 3 };

        const auto* src_hdr   { header.size() >= sizeof(crxg_header) ? crxg_header::cast(header.data()) : nullptr };
        const bool  has_header{ src_hdr != nullptr && src_hdr->is_valid() };

        const int      bpp     { has_header ? src_hdr->bpp()    : 3 };
        const uint16_t crx_type{ has_header ? src_hdr->crx_type : static_cast<uint16_t>(2) };
        const uint16_t mode    { has_header ? src_hdr->mode     : static_cast<uint16_t>(0) };

        int8_t src_r_off{ 0 }, src_g_off{ 1 }, src_b_off{ 2 }, src_a_off{ 3 };
        switch (data.format)
        {
        case pixels::bgra: src_b_off = 0; src_g_off = 1; src_r_off = 2; src_a_off = 3; break;
        case pixels::argb: src_a_off = 0; src_r_off = 1; src_g_off = 2; src_b_off = 3; break;
        case pixels::abgr: src_a_off = 0; src_b_off = 1; src_g_off = 2; src_r_off = 3; break;
        default: break;
        }

        const int src_stride   { data.width * channels };
        const int padded_stride{ ((data.width * bpp + 3) / 4) * 4 };

        std::vector<uint8_t> pixels_buf{};
        pixels_buf.resize(static_cast<size_t>(padded_stride) * data.height);

        const uint8_t alpha_xor{ static_cast<uint8_t>((bpp == 4 && mode != 2) ? 0xFF : 0) };
        for (int y{}; y < data.height; ++y)
        {
            const uint8_t* src_row{ data.data.data() + static_cast<size_t>(y) * src_stride };
            uint8_t*       dst_row{ pixels_buf.data() + static_cast<size_t>(y) * padded_stride };

            for (int x{}; x < data.width; ++x)
            {
                const uint8_t* src{ src_row + static_cast<size_t>(x) * channels };
                uint8_t*       dst{ dst_row + static_cast<size_t>(x) * bpp };

                if (bpp == 4)
                {
                    dst[0] = static_cast<uint8_t>(src[src_a_off] ^ alpha_xor);
                    dst[1] = src[src_b_off];
                    dst[2] = src[src_g_off];
                    dst[3] = src[src_r_off];
                }
                else
                {
                    dst[0] = src[src_b_off];
                    dst[1] = src[src_g_off];
                    dst[2] = src[src_r_off];
                }
            }
        }

        std::vector<uint8_t> packed{};
        const std::span<const uint8_t> pixel_span{ pixels_buf };
        {
            if (crx_type == 1)
            {
                if (!crx::pack_v1(pixel_span, data.width, data.height, bpp, packed))
                {
                    return false;
                }
            }
            else
            {
                if (!crx::pack_v2(pixel_span, data.width, data.height, bpp, packed))
                {
                    return false;
                }
            }
        }

        const size_t prefix_size{ has_header ? header.size() : sizeof(crxg_header) };
        
        buffer.clear();
        buffer.resize(prefix_size + packed.size());
        if (has_header)
        {
            std::memcpy(buffer.data(), header.data(), header.size());
            auto* const out_hdr{ reinterpret_cast<crxg_header*>(buffer.data()) };
            out_hdr->width  = static_cast<uint16_t>(data.width);
            out_hdr->height = static_cast<uint16_t>(data.height);
        }
        else
        {
            auto* const hdr{ reinterpret_cast<crxg_header*>(buffer.data()) };
            hdr->magic    = crxg_header::crxg_magic;
            hdr->offset_x = 0;
            hdr->offset_y = 0;
            hdr->width    = static_cast<uint16_t>(data.width);
            hdr->height   = static_cast<uint16_t>(data.height);
            hdr->crx_type = 2;
            hdr->flags    = 1;
            hdr->colors   = 0;
            hdr->mode     = 0;
        }

        std::memcpy(buffer.data() + prefix_size, packed.data(), packed.size());

        return true;
    }

}