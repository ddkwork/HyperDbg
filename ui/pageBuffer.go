package ui

import (
	"io"

	"github.com/ddkwork/golibrary/std/mylog"
)

const pageSize = 1024 * 1024

type PagedWriter struct {
	pages []*[pageSize]byte
	last  *[pageSize]byte
	off   int
}

func NewPagedWriter() *PagedWriter {
	return &PagedWriter{last: new([pageSize]byte)}
}

func (p *PagedWriter) Write(buf []byte) (written int, err error) {
	for {
		n := copy(p.last[p.off:], buf[written:])
		written += n
		if written >= len(buf) {
			p.off += n
			return
		}
		p.pages = append(p.pages, p.last)
		p.last, p.off = new([pageSize]byte), 0
	}
}

func (p *PagedWriter) Len() int {
	return len(p.pages)*pageSize + p.off
}

func (p *PagedWriter) Bytes() []byte {
	out, n := make([]byte, p.Len()), 0
	for _, page := range p.pages {
		n += copy(out[n:], page[:])
	}
	copy(out[n:], p.last[:p.off])
	return out
}

func (p *PagedWriter) ToReader() *PagedReader {
	return &PagedReader{src: p, curr: p.getPage(0)}
}

func (p *PagedWriter) getPage(ipage int) []byte {
	if ipage == len(p.pages) { // last page
		return p.last[:p.off]
	}
	return p.pages[ipage][:]
}

type PagedReader struct {
	src   *PagedWriter
	curr  []byte
	off   int
	ipage int
}

func (p *PagedReader) WriteTo(w io.Writer) (written int64, err error) {
	n := mylog.Check2(w.Write(p.curr[p.off:]))
	written = int64(n)

	src, ipage := p.src, p.ipage
	for {
		if ipage == len(src.pages) { // last page
			p.ipage, p.off = ipage, len(p.curr)
			return
		}
		ipage++
		//page := src.getPage(ipage)//todo: optimize this
		//n, err = w.Write(page)
		//written += int64(n)
		//if err != nil {
		//	p.ipage, p.curr, p.off = ipage, page, n
		//	return
		//}
	}
}

func (p *PagedReader) Read(buf []byte) (nread int, err error) {
	for {
		n := copy(buf[nread:], p.curr[p.off:])
		nread += n
		p.off += n
		if nread >= len(buf) {
			return
		}
		src := p.src
		if p.ipage == len(src.pages) { // last page
			err = io.EOF
			return
		}
		p.ipage++
		// p.curr, p.off = src.getPage(p.ipage), 0//todo: optimize this
	}
}
