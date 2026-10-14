/*****************************************************************************
 * firstrun.cpp : First Run dialogs
 ****************************************************************************
 * Copyright © 2009 VideoLAN
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/* Reframe changes, 2026-10-13: centered privacy choice before library onboarding. */

#include "dialogs/firstrun.hpp"

#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>

FirstRun::FirstRun( QWidget *_p, intf_thread_t *_p_intf  )
         : Reframe::Dialog( _p, qtr("Privacy and Network Access"), QSize(580, 440)), p_intf( _p_intf )
{
    setObjectName("privacyDialog");
    buildPrivDialog();
    Reframe::centerOnParent(this);
}

void FirstRun::save()
{
    config_PutInt( p_intf,  "metadata-network-access", checkbox->isChecked() );
#ifdef UPDATE_CHECK
    config_PutInt( p_intf,  "qt-updates-notif", checkbox2->isChecked() );
#endif
    config_PutInt( p_intf,  "qt-privacy-ask", 0 );

    /* FIXME Should not save here. This will not work as expected if another
     * plugin overwrote items of its own. */
#warning FIXME
    /* We have to save here because the user may not launch Prefs */
    config_SaveConfigFile( p_intf );
    accept();
}

void FirstRun::buildPrivDialog()
{
    setWindowRole( "reframe-privacy" );
    setWindowModality( Qt::ApplicationModal );
    QLabel *text = new QLabel( qtr(
        "<p>Reframe can look up cover art, track names, artists and other media information "
        "using third-party Internet services.</p>"
        "<p>These requests may identify media in your library to those services. "
        "Choose whether to allow automatic metadata lookups before setting up your library.</p>"
        "<p>You can change this choice later in Settings &gt; Privacy.</p>"
        ) );
    text->setWordWrap( true );
    text->setTextFormat( Qt::RichText );

    body->addWidget(text);
    checkbox = new QCheckBox( qtr( "Allow metadata network access" ) );
    checkbox->setObjectName("metadataNetworkAccess");
    checkbox->setChecked( var_InheritBool(p_intf, "metadata-network-access") );
    body->addWidget(checkbox);

#ifdef UPDATE_CHECK
    checkbox2 = new QCheckBox( qtr( "Regularly check for VLC updates" ) );
    checkbox2->setChecked( true );
    body->addWidget(checkbox2);
#endif

    QDialogButtonBox *buttonsBox = new QDialogButtonBox( this );
    buttonsBox->addButton( qtr( "Continue" ), QDialogButtonBox::AcceptRole );

    body->addStretch();
    body->addWidget( buttonsBox );

    connect( buttonsBox, &QDialogButtonBox::accepted, this, &FirstRun::save );
    buttonsBox->setFocus();
}
