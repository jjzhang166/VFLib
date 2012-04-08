// Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file BOOST_LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// This file incorporates work covered by the following copyright
// and permission notice:
//
//   Copyright (C) 2004-2008 Maciej Sobczak, Stephen Hutton,
//   Mateusz Loskot, Pawel Aleksander Fedorynski, David Courtney,
//   Rafal Bobrowski, Julian Taylor, Henning Basold, Ilia Barahovski,
//   Denis Arnaud, Daniel Lidstr�m, Matthieu Kermagoret, Artyom Beilis,
//   Cory Bennett, Chris Weed, Michael Davidsaver, Jakub Stachowski,
//   Alex Ott, Rainer Bauer, Martin Muenstermann, Philip Pemberton,
//   Eli Green, Frederic Chateau, Artyom Tonkikh, Roger Orr,
//   Robert Massaioli, Sergey Nikulov, Shridhar Daithankar,
//   S�ren Meyer-Eppler, Mario Valesco
//
//   Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file BOOST_LICENSE_1_0.txt or copy at
//   http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef VF_DB_DETAIL_REF_COUNTED_PREPARE_INFO_VFHEADER
#define VF_DB_DETAIL_REF_COUNTED_PREPARE_INFO_VFHEADER

namespace db {

namespace detail {

// helper that collects statement text and bindings to create the statement
class ref_counted_prepare_info : public ref_counted_statement_base
{
public:
  ref_counted_prepare_info (session& s);
  void exchange (into_type_ptr const& i);
  void exchange (use_type_ptr const& u);
  virtual void final_action();

public:
  std::string get_query() const;

  std::vector <into_type_base*> m_intos;
  std::vector <use_type_base*> m_uses;
};

}

}

#endif
