/***************************************************************************
 *   Copyright (C) 2010 by Andrey Afletdinov <fheroes2@gmail.com>          *
 *                                                                         *
 *   Part of the Free Heroes2 Engine:                                      *
 *   http://sourceforge.net/projects/fheroes2                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "agg.h"
#include "agg_image.h"
#include "castle.h"
#include "cursor.h"
#include "game.h"
#include "game_interface.h"
#include "heroes.h"
#include "icn.h"
#include "interface_list.h"
#include "kingdom.h"
#include "logging.h"
#include "m82.h"
#include "monster.h"
#include "rand.h"
#include "spell.h"
#include "text.h"
#include "ui_window.h"
#include "world.h"

#include <cassert>
#include <memory>

namespace
{
    // Values are extracted from Heroes2 executable
    const uint32_t dimensionDoorPenalty = 225;
    const uint32_t townGatePenalty = 225;
}

void DialogSpellFailed( const Spell & spell );

bool ActionSpellViewMines( const Heroes & hero );
bool ActionSpellViewResources( const Heroes & hero );
bool ActionSpellViewArtifacts( const Heroes & hero );
bool ActionSpellViewTowns( const Heroes & hero );
bool ActionSpellViewHeroes( const Heroes & hero );
bool ActionSpellViewAll( const Heroes & hero );
bool ActionSpellIdentifyHero( const Heroes & hero );
bool ActionSpellSummonBoat( const Heroes & hero );
bool ActionSpellDimensionDoor( Heroes & hero );
bool ActionSpellTownGate( Heroes & hero );
bool ActionSpellTownPortal( Heroes & hero );
bool ActionSpellVisions( Heroes & hero );
bool ActionSpellSetGuardian( Heroes & hero, const Spell & spell );

class CastleIndexListBox : public Interface::ListBox<s32>
{
public:
    CastleIndexListBox( const fheroes2::Point & pt, int & res, const bool isEvilInterface )
        : Interface::ListBox<s32>( pt )
        , result( res )
        , _townFrameIcnId( isEvilInterface ? ICN::ADVBORDE : ICN::ADVBORD )
        , _listBoxIcnId( isEvilInterface ? ICN::LISTBOX_EVIL : ICN::LISTBOX )
    {}

    void RedrawItem( const s32 &, s32, s32, bool ) override;
    void RedrawBackground( const fheroes2::Point & ) override;

    void ActionCurrentUp( void ) override {}

    void ActionCurrentDn( void ) override {}

    void ActionListDoubleClick( s32 & ) override
    {
        result = Dialog::OK;
    }

    void ActionListSingleClick( s32 & ) override {}

    void ActionListPressRight( int32_t & index ) override
    {
        const Castle * castle = world.GetCastle( Maps::GetPoint( index ) );

        if ( castle != nullptr ) {
            Dialog::QuickInfo( *castle );
        }
    }

    int & result;

private:
    int _townFrameIcnId;
    int _listBoxIcnId;
};

void CastleIndexListBox::RedrawItem( const s32 & index, s32 dstx, s32 dsty, bool current )
{
    const Castle * castle = world.GetCastle( Maps::GetPoint( index ) );

    if ( castle ) {
        fheroes2::Blit( fheroes2::AGG::GetICN( _townFrameIcnId, 0 ), 481, 177, fheroes2::Display::instance(), dstx, dsty, 54, 30 );
        Interface::RedrawCastleIcon( *castle, dstx + 4, dsty + 4 );
        Text text( castle->GetName(), ( current ? Font::YELLOW_BIG : Font::BIG ) );

        if ( VisibleItemCount() > 0 ) {
            const int32_t heightPerItem = ( rtAreaItems.height - VisibleItemCount() ) / VisibleItemCount();
            text.Blit( dstx + 60, dsty + ( heightPerItem - text.h() ) / 2, 196 );
        }
        else {
            assert( 0 ); // this should never happen!
            text.Blit( dstx + 60, dsty, 196 );
        }
    }
}

void CastleIndexListBox::RedrawBackground( const fheroes2::Point & dst )
{
    fheroes2::Display & display = fheroes2::Display::instance();

    Text text( _( "Town Portal" ), Font::YELLOW_BIG );
    text.Blit( dst.x + 140 - text.w() / 2, dst.y + 5 );

    text.Set( _( "Select town to port to." ), Font::BIG );
    text.Blit( dst.x + 140 - text.w() / 2, dst.y + 25 );

    const fheroes2::Sprite & upperPart = fheroes2::AGG::GetICN( _listBoxIcnId, 0 );
    const fheroes2::Sprite & middlePart = fheroes2::AGG::GetICN( _listBoxIcnId, 1 );
    const fheroes2::Sprite & lowerPart = fheroes2::AGG::GetICN( _listBoxIcnId, 2 );

    int32_t offsetY = 45;
    fheroes2::Blit( upperPart, display, dst.x + 2, dst.y + offsetY );

    offsetY += upperPart.height();

    int32_t totalHeight = rtAreaItems.height + 6;
    int32_t middlePartCount = ( totalHeight - upperPart.height() - lowerPart.height() + middlePart.height() - 1 ) / middlePart.height();

    for ( int32_t i = 0; i < middlePartCount; ++i ) {
        fheroes2::Blit( fheroes2::AGG::GetICN( _listBoxIcnId, 1 ), display, dst.x + 2, dst.y + offsetY );
        offsetY += middlePart.height();
    }

    fheroes2::Blit( lowerPart, display, dst.x + 2, dst.y + totalHeight - lowerPart.height() + 45 );

    const fheroes2::Sprite & upperScrollbarArrow = fheroes2::AGG::GetICN( _listBoxIcnId, 3 );
    const fheroes2::Sprite & lowerScrollbarArrow = fheroes2::AGG::GetICN( _listBoxIcnId, 5 );

    totalHeight = rtAreaItems.height + 8 - upperScrollbarArrow.height() - lowerScrollbarArrow.height();

    const fheroes2::Sprite & upperScrollbar = fheroes2::AGG::GetICN( _listBoxIcnId, 7 );
    const fheroes2::Sprite & middleScrollbar = fheroes2::AGG::GetICN( _listBoxIcnId, 8 );
    const fheroes2::Sprite & lowerScrollbar = fheroes2::AGG::GetICN( _listBoxIcnId, 9 );

    offsetY = upperScrollbarArrow.height() + 44;
    fheroes2::Blit( upperScrollbar, display, dst.x + 256, dst.y + offsetY );
    offsetY += upperScrollbar.height();

    middlePartCount = ( totalHeight - upperScrollbar.height() - lowerScrollbar.height() + middleScrollbar.height() - 1 ) / middleScrollbar.height();

    for ( int32_t i = 0; i < middlePartCount; ++i ) {
        fheroes2::Blit( middleScrollbar, display, dst.x + 256, dst.y + offsetY );
        offsetY += middleScrollbar.height();
    }

    offsetY = upperScrollbarArrow.height() + 44 + totalHeight - lowerScrollbar.height();
    fheroes2::Blit( lowerScrollbar, display, dst.x + 256, dst.y + offsetY );
}

bool Heroes::ActionSpellCast( const Spell & spell )
{
    std::string error;

    if ( !CanMove() && ( spell == Spell::DIMENSIONDOOR || spell == Spell::TOWNGATE || spell == Spell::TOWNPORTAL ) ) {
        Dialog::Message( "", _( "Your hero is too tired to cast this spell today. Try again tomorrow." ), Font::BIG, Dialog::OK );
        return false;
    }
    else if ( spell == Spell::NONE || spell.isCombat() || !CanCastSpell( spell, &error ) ) {
        if ( error.size() )
            Dialog::Message( "Error", error, Font::BIG, Dialog::OK );
        return false;
    }

    bool apply = false;

    switch ( spell() ) {
    case Spell::VIEWMINES:
        apply = ActionSpellViewMines( *this );
        break;
    case Spell::VIEWRESOURCES:
        apply = ActionSpellViewResources( *this );
        break;
    case Spell::VIEWARTIFACTS:
        apply = ActionSpellViewArtifacts( *this );
        break;
    case Spell::VIEWTOWNS:
        apply = ActionSpellViewTowns( *this );
        break;
    case Spell::VIEWHEROES:
        apply = ActionSpellViewHeroes( *this );
        break;
    case Spell::VIEWALL:
        apply = ActionSpellViewAll( *this );
        break;
    case Spell::IDENTIFYHERO:
        apply = ActionSpellIdentifyHero( *this );
        break;
    case Spell::SUMMONBOAT:
        apply = ActionSpellSummonBoat( *this );
        break;
    case Spell::DIMENSIONDOOR:
        apply = ActionSpellDimensionDoor( *this );
        break;
    case Spell::TOWNGATE:
        apply = isShipMaster() ? false : ActionSpellTownGate( *this );
        break;
    case Spell::TOWNPORTAL:
        apply = isShipMaster() ? false : ActionSpellTownPortal( *this );
        break;
    case Spell::VISIONS:
        apply = ActionSpellVisions( *this );
        break;
    case Spell::HAUNT:
        apply = ActionSpellSetGuardian( *this, spell );
        break;
    case Spell::SETEGUARDIAN:
        apply = ActionSpellSetGuardian( *this, spell );
        break;
    case Spell::SETAGUARDIAN:
        apply = ActionSpellSetGuardian( *this, spell );
        break;
    case Spell::SETFGUARDIAN:
        apply = ActionSpellSetGuardian( *this, spell );
        break;
    case Spell::SETWGUARDIAN:
        apply = ActionSpellSetGuardian( *this, spell );
        break;
    default:
        break;
    }

    if ( apply ) {
        DEBUG_LOG( DBG_GAME, DBG_INFO, GetName() << " cast spell: " << spell.GetName() );
        SpellCasted( spell );
        return true;
    }
    return false;
}

bool HeroesTownGate( Heroes & hero, const Castle * castle )
{
    if ( castle ) {
        Interface::Basic & I = Interface::Basic::Get();

        const s32 src = hero.GetIndex();
        const s32 dst = castle->GetIndex();

        if ( !Maps::isValidAbsIndex( src ) || !Maps::isValidAbsIndex( dst ) )
            return false;

        AGG::PlaySound( M82::KILLFADE );
        hero.GetPath().Hide();
        hero.FadeOut();

        hero.ApplyPenaltyMovement( townGatePenalty );
        hero.Move2Dest( dst );

        I.GetGameArea().SetCenter( hero.GetCenter() );
        I.RedrawFocus();
        I.Redraw();

        AGG::PlaySound( M82::KILLFADE );
        hero.FadeIn();
        hero.GetPath().Reset();
        hero.GetPath().Show(); // Reset method sets Hero's path to hidden mode with non empty path, we have to set it back

        I.SetFocus( &hero );

        // educate spells
        castle->MageGuildEducateHero( hero );

        return true;
    }
    return false;
}

void DialogSpellFailed( const Spell & spell )
{
    // failed
    std::string str = _( "%{spell} failed!!!" );
    StringReplace( str, "%{spell}", spell.GetName() );
    Dialog::Message( "", str, Font::BIG, Dialog::OK );
}

bool ActionSpellViewMines( const Heroes & )
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewMines, Interface::Basic::Get() );
    return true;
}

bool ActionSpellViewResources( const Heroes & )
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewResources, Interface::Basic::Get() );
    return true;
}

bool ActionSpellViewArtifacts( const Heroes & )
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewArtifacts, Interface::Basic::Get() );
    return true;
}

bool ActionSpellViewTowns( const Heroes & )
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewTowns, Interface::Basic::Get() );
    return true;
}

bool ActionSpellViewHeroes( const Heroes & )
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewHeroes, Interface::Basic::Get() );
    return true;
}

bool ActionSpellViewAll( const Heroes & )
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewAll, Interface::Basic::Get() );
    return true;
}

bool ActionSpellIdentifyHero( const Heroes & hero )
{
    if ( hero.GetKingdom().Modes( Kingdom::IDENTIFYHERO ) ) {
        Message( "", _( "This spell is already in use." ), Font::BIG, Dialog::OK );
        return false;
    }

    hero.GetKingdom().SetModes( Kingdom::IDENTIFYHERO );
    Message( "", _( "Enemy heroes are now fully identifiable." ), Font::BIG, Dialog::OK );

    return true;
}

bool ActionSpellSummonBoat( const Heroes & hero )
{
    if ( hero.isShipMaster() ) {
        Dialog::Message( "", _( "This spell cannot be used on a boat." ), Font::BIG, Dialog::OK );
        return false;
    }

    const int32_t center = hero.GetIndex();
    const fheroes2::Point & centerPoint = Maps::GetPoint( center );

    // find water
    int32_t dst_water = -1;
    MapsIndexes freeTiles = Maps::ScanAroundObject( center, MP2::OBJ_ZERO );
    std::sort( freeTiles.begin(), freeTiles.end(), [&centerPoint]( const int32_t left, const int32_t right ) {
        const fheroes2::Point & leftPoint = Maps::GetPoint( left );
        const fheroes2::Point & rightPoint = Maps::GetPoint( right );
        const int32_t leftDiffX = leftPoint.x - centerPoint.x;
        const int32_t leftDiffY = leftPoint.y - centerPoint.y;
        const int32_t rightDiffX = rightPoint.x - centerPoint.x;
        const int32_t rightDiffY = rightPoint.y - centerPoint.y;

        return ( leftDiffX * leftDiffX + leftDiffY * leftDiffY ) < ( rightDiffX * rightDiffX + rightDiffY * rightDiffY );
    } );

    for ( MapsIndexes::const_iterator it = freeTiles.begin(); it != freeTiles.end(); ++it ) {
        if ( world.GetTiles( *it ).isWater() ) {
            dst_water = *it;
            break;
        }
    }

    if ( !Maps::isValidAbsIndex( dst_water ) ) {
        Dialog::Message( "", _( "This spell can be casted only nearby water." ), Font::BIG, Dialog::OK );
        return false;
    }

    const MapsIndexes & boats = Maps::GetObjectPositions( center, MP2::OBJ_BOAT, false );
    for ( auto it = boats.cbegin(); it != boats.cend(); ++it ) {
        if ( Maps::isValidAbsIndex( *it ) ) {
            const uint32_t distance = Maps::GetApproximateDistance( *it, hero.GetIndex() );
            if ( distance > 1 ) {
                Game::ObjectFadeAnimation::PrepareFadeTask( MP2::OBJ_BOAT, *it, dst_water, true, true );
                Game::ObjectFadeAnimation::PerformFadeTask();

                return true;
            }
        }
    }

    DialogSpellFailed( Spell::SUMMONBOAT );
    return true;
}

bool ActionSpellDimensionDoor( Heroes & hero )
{
    const u32 distance = Spell::CalculateDimensionDoorDistance( hero.GetPower(), hero.GetArmy().GetHitPoints() );

    Interface::Basic & I = Interface::Basic::Get();

    // center hero
    I.GetGameArea().SetCenter( hero.GetCenter() );
    I.RedrawFocus();
    I.Redraw();

    const s32 src = hero.GetIndex();
    // get destination
    const s32 dst = I.GetDimensionDoorDestination( src, distance, hero.isShipMaster() );

    if ( Maps::isValidAbsIndex( src ) && Maps::isValidAbsIndex( dst ) ) {
        AGG::PlaySound( M82::KILLFADE );
        hero.GetPath().Hide();
        hero.FadeOut();

        hero.SpellCasted( Spell::DIMENSIONDOOR );

        hero.ApplyPenaltyMovement( dimensionDoorPenalty );
        hero.Move2Dest( dst );

        I.GetGameArea().SetCenter( hero.GetCenter() );
        I.RedrawFocus();
        I.Redraw();

        AGG::PlaySound( M82::KILLFADE );
        hero.FadeIn();
        hero.GetPath().Reset();
        hero.GetPath().Show(); // Reset method sets Hero's path to hidden mode with non empty path, we have to set it back

        // No action is being made. Uncomment this code if the logic will be changed
        // hero.ActionNewPosition();

        I.ResetFocus( GameFocus::HEROES );

        return false; /* SpellCasted apply */
    }

    return false;
}

bool ActionSpellTownGate( Heroes & hero )
{
    const Kingdom & kingdom = hero.GetKingdom();
    const KingdomCastles & castles = kingdom.GetCastles();
    KingdomCastles::const_iterator it;

    const Castle * castle = NULL;
    const s32 center = hero.GetIndex();
    s32 min = -1;

    // find the nearest castle
    for ( it = castles.begin(); it != castles.end(); ++it )
        if ( *it ) {
            int min2 = Maps::GetApproximateDistance( center, ( *it )->GetIndex() );
            if ( 0 > min || min2 < min ) {
                min = min2;
                castle = *it;
            }
        }

    Interface::Basic & I = Interface::Basic::Get();

    // center hero
    I.GetGameArea().SetCenter( hero.GetCenter() );
    I.RedrawFocus();
    I.Redraw();

    if ( !castle ) {
        Dialog::Message( "", _( "No available towns.\nSpell Failed!!!" ), Font::BIG, Dialog::OK );
        return false;
    }
    else if ( castle->GetHeroes().Guest() && castle->GetHeroes().Guest() != &hero ) {
        Dialog::Message( "", _( "Nearest town occupied.\nSpell Failed!!!" ), Font::BIG, Dialog::OK );
        return false;
    }

    return HeroesTownGate( hero, castle );
}

bool ActionSpellTownPortal( Heroes & hero )
{
    const Kingdom & kingdom = hero.GetKingdom();
    std::vector<s32> castles;

    fheroes2::Display & display = fheroes2::Display::instance();

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    const bool isEvilInterface = Settings::Get().ExtGameEvilInterface();
    LocalEvent & le = LocalEvent::Get();

    for ( KingdomCastles::const_iterator it = kingdom.GetCastles().begin(); it != kingdom.GetCastles().end(); ++it )
        if ( *it && !( *it )->GetHeroes().Guest() )
            castles.push_back( ( **it ).GetIndex() );

    if ( castles.empty() ) {
        Dialog::Message( "", _( "No available towns.\nSpell Failed!!!" ), Font::BIG, Dialog::OK );
        return false;
    }

    std::unique_ptr<fheroes2::StandardWindow> frameborder( new fheroes2::StandardWindow( 280, 250 ) );

    const fheroes2::Rect area( frameborder->activeArea() );
    int result = Dialog::ZERO;

    CastleIndexListBox listbox( area.getPosition(), result, isEvilInterface );

    const int listId = isEvilInterface ? ICN::LISTBOX_EVIL : ICN::LISTBOX;
    listbox.SetScrollButtonUp( listId, 3, 4, fheroes2::Point( area.x + 256, area.y + 45 ) );
    listbox.SetScrollButtonDn( listId, 5, 6, fheroes2::Point( area.x + 256, area.y + 190 ) );
    listbox.SetScrollBar( fheroes2::AGG::GetICN( listId, 10 ), fheroes2::Rect( area.x + 260, area.y + 68, 14, 119 ) );
    listbox.SetAreaMaxItems( 5 );
    listbox.SetAreaItems( fheroes2::Rect( area.x + 6, area.y + 49, 250, 160 ) );
    listbox.SetListContent( castles );
    listbox.RedrawBackground( area.getPosition() );
    listbox.Redraw();

    fheroes2::ButtonGroup btnGroups;
    btnGroups.createButton( area.x, area.y + 222, isEvilInterface ? ICN::NON_UNIFORM_EVIL_OKAY_BUTTON : ICN::NON_UNIFORM_GOOD_OKAY_BUTTON, 0, 1, Dialog::OK );
    btnGroups.createButton( area.x + 182, area.y + 222, isEvilInterface ? ICN::NON_UNIFORM_EVIL_CANCEL_BUTTON : ICN::NON_UNIFORM_GOOD_CANCEL_BUTTON, 0, 1,
                            Dialog::CANCEL );
    btnGroups.draw();

    display.render();

    while ( result == Dialog::ZERO && le.HandleEvents() ) {
        result = btnGroups.processEvents();
        listbox.QueueEventProcessing();

        if ( !listbox.IsNeedRedraw() ) {
            continue;
        }

        listbox.Redraw();
        display.render();
    }
    frameborder.reset();
    // store
    if ( result == Dialog::OK )
        return HeroesTownGate( hero, world.GetCastle( Maps::GetPoint( listbox.GetCurrent() ) ) );

    return false;
}

bool ActionSpellVisions( Heroes & hero )
{
    const u32 dist = hero.GetVisionsDistance();
    MapsIndexes monsters = Maps::ScanAroundObject( hero.GetIndex(), dist, MP2::OBJ_MONSTER );

    const int32_t heroColor = hero.GetColor();
    monsters.resize( std::distance( monsters.begin(), std::remove_if( monsters.begin(), monsters.end(),
                                                                      [heroColor]( const int32_t index ) { return world.GetTiles( index ).isFog( heroColor ); } ) ) );

    if ( monsters.empty() ) {
        std::string msg = _( "You must be within %{count} spaces of a monster for the Visions spell to work." );
        StringReplace( msg, "%{count}", dist );
        Dialog::Message( "", msg, Font::BIG, Dialog::OK );
        return false;
    }

    for ( MapsIndexes::const_iterator it = monsters.begin(); it != monsters.end(); ++it ) {
        const Maps::Tiles & tile = world.GetTiles( *it );
        const MapMonster * map_troop = NULL;
        if ( tile.GetObject() == MP2::OBJ_MONSTER )
            map_troop = dynamic_cast<MapMonster *>( world.GetMapObject( tile.GetObjectUID() ) );

        Troop troop = map_troop ? map_troop->QuantityTroop() : tile.QuantityTroop();
        const JoinCount join = Army::GetJoinSolution( hero, tile, troop );

        std::string hdr;
        std::string msg;

        hdr = std::string( "%{count} " ) + troop.GetPluralName( join.second );
        StringReplace( hdr, "%{count}", troop.GetCount() );

        switch ( join.first ) {
        default:
            msg = _( "I fear these creatures are in the mood for a fight." );
            break;

        case JOIN_FREE:
            msg = _( "The creatures are willing to join us!" );
            break;

        case JOIN_COST:
            if ( join.second == troop.GetCount() )
                msg = _( "All the creatures will join us..." );
            else {
                msg = _n( "The creature will join us...", "%{count} of the creatures will join us...", join.second );
                StringReplace( msg, "%{count}", join.second );
            }
            msg.append( "\n" );
            msg.append( "\n for a fee of %{gold} gold." );
            StringReplace( msg, "%{gold}", troop.GetCost().gold );
            break;

        case JOIN_FLEE:
            msg = _( "These weak creatures will surely flee before us." );
            break;
        }

        Dialog::Message( hdr, msg, Font::BIG, Dialog::OK );
    }

    hero.SetModes( Heroes::VISIONS );

    return true;
}

bool ActionSpellSetGuardian( Heroes & hero, const Spell & spell )
{
    Maps::Tiles & tile = world.GetTiles( hero.GetIndex() );

    if ( MP2::OBJ_MINES != tile.GetObject( false ) ) {
        Dialog::Message( "", _( "You must be standing on the entrance to a mine (sawmills and alchemists don't count) to cast this spell." ), Font::BIG, Dialog::OK );
        return false;
    }

    const u32 count = hero.GetPower() * spell.ExtraValue();

    if ( count ) {
        tile.SetQuantity3( spell() );

        if ( spell == Spell::HAUNT ) {
            world.CaptureObject( tile.GetIndex(), Color::UNUSED );
            hero.SetMapsObject( MP2::OBJ_ABANDONEDMINE );
        }

        world.GetCapturedObject( tile.GetIndex() ).GetTroop().Set( Monster( spell ), count );
        return true;
    }

    return false;
}
