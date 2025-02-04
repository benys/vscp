// duktape_vscp_wrkthread.h
//
// This file is part of the VSCP (https://www.vscp.org)
//
// The MIT License (MIT)
//
// Copyright © 2000-2020 Ake Hedman, Grodans Paradis AB
// <info@grodansparadis.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#if !defined(VSCP_JAVASCRIPT__INCLUDED_)
#define VSCP_JAVASCRIPT__INCLUDED_

#include <vscpdatetime.h>

////////////////////////////////////////////////////////////////////////////////
// actionJavascriptObj
//

class actionJavascriptObj
{

  public:
    /// Constructor
    actionJavascriptObj(std::string &strScript);

    /// Destructor
    virtual ~actionJavascriptObj();

    /*!
        Termination control
     */
    bool m_bQuit;

    /*!
     * Script
     */
    std::string m_strScript;

    /// JavaScript executing id
    uint64_t m_id;

    /// Time when script was started
    vscpdatetime m_start;

    /// Time when script was stopped
    vscpdatetime m_stop;

    /// Client item for script
    CClientItem *m_pClientItem;

    /// Feed event
    vscpEventEx m_feedEvent;
};

#endif