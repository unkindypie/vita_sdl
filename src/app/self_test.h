#pragma once
#include "scripting/runtime.h"
#include "audio/mixer.h"
#include "game_api.h"
#include "audio.h"
#include "game.h"
#include "platform.h"
#include "platform/engine_hooks.h"
#include "games/asciicker/projectiles.h"
#include "mainmenu.h"
#include <cmath>
extern World* world;
extern Character* player_head;
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
extern const char* GetConfPath();
inline void gameplay_checks(const std::string& root) {
    auto check = [](bool value, const char* message) { if (!value) throw std::runtime_error(message); };
    std::string result;
    auto js = [&](const std::string& code) { check(port::eval_script(code, true, result), result.c_str()); };
    js("if(akAPI_This.frames<90)throw Error('onFrame did not run');"
       "if(typeof Atomics!=='undefined'||typeof SharedArrayBuffer!=='undefined')throw Error('unsupported atomics exposed');"
       "ak.setName('Vita 🏳️‍⚧️');if(ak.getName()!=='Vita 🏳️‍⚧️')throw Error('Unicode');"
       "ak.setWater(57);if(ak.getWater()!==57)throw Error('water');"
       "ak.setMove([1,2,3]);let movement=[];ak.getMove(movement);if(movement.join(',')!=='1,2,3')throw Error('move');"
       "ak.setMove([0,0,0]);ak.setLight([1,0.5,0.25,1]);let light=[];ak.getLight(light);if(light[2]!==0.25)throw Error('light');"
       "ak.jump();if(typeof ak.isGrounded()!=='boolean')throw Error('grounded');"
       "ak.onSay(function(text){this.said=text;return false;});");
    check(game->input.jump, "script jump reaches engine");
    bool allowed = true;
    check(akAPI_OnSay("hello", 5, &allowed) && !allowed, "onSay boolean gate");
    js("if(akAPI_This.said!=='hello')throw Error('onSay data');ak.onSay(null)");
    check(!akAPI_OnSay("hello", 5, &allowed), "callback unregister");
    for (const auto& callback : {"return 42", "return 'custom'", "return [42,'custom']", "return {story_id:42,desc:'custom'}", "return null", "return false"}) {
        js(std::string("ak.onItem(function(action,story,kind,subkind,weight,desc){") + callback + "})");
        int story = -1;
        const char* description = nullptr;
        allowed = true;
        check(akAPI_OnItem(1, 7, 'W', 1, 5, "original", &allowed, &story, &description), "onItem dispatched");
        if (std::strstr(callback, "false")) check(!allowed && story == 7, "onItem rejects");
        else {
            check(allowed, "onItem allows");
            check(story == (std::strstr(callback, "42") ? 42 : 7), "onItem story layout");
            check(description && std::string(description) == (std::strstr(callback, "custom") ? "custom" : "original"), "onItem description layout");
        }
    }
    js("ak.onItem(null);ak.say('script speech');");
    check(game->player.talks > 0, "script speech creates a bubble");
    check(!port::eval_script("throw new Error('expected')", true, result), "script exceptions reported");
    check(!port::eval_script("while(true){}", true, result), "infinite script interrupted");
    js("ak.onFrame(function(){while(true){}})");
    const auto errors = port::script_stats().errors;
    check(akAPI_OnFrame(), "failing callback attempted");
    check(port::script_stats().errors == errors + 1 && !akAPI_OnFrame(), "failing callback disabled");
    js("ak.onFrame(null);1+1");
    check(result == "2", "runtime usable after interruption");
    check(!port::eval_script("stringToUTF8('bad',65535,2)", true, result), "API string bounds");
    js("ak.setName('x'.repeat(126)+'😀');if(ak.getName()!=='x'.repeat(126))throw Error('name UTF8 boundary');ak.setName('player')");
    check(!port::eval_script("ak.setPos([NaN,0,0])", true, result), "non-finite native values rejected");
    check(!port::eval_script("ak.setMount(9999)", true, result), "mount bounds");
    check(!port::eval_script("(function recurse(){return recurse()})()", true, result), "stack limit");
    check(!port::eval_script("new ArrayBuffer(32*1024*1024)", true, result), "memory limit");
    js("2+2"); check(result == "4", "runtime usable after stack/OOM exceptions");
    js("ak.setName('player');ak.setWater(55);ak.setLight([1,0,1,0.5]);");

    // Real upstream controller actions, not copied gameplay logic.
    game->OnPadMount(true);
    game->OnPadButton(9, true); game->OnPadButton(9, false);
    check(game->show_inventory, "controller opens inventory");
    game->OnPadButton(9, true); game->OnPadButton(9, false);
    check(!game->show_inventory, "controller closes inventory");
    game->OpenMenu(0);
    check(game->menu_depth == 0, "menu opens");
    game->OnKeyb(KEYB_DOWN, A3D_ESCAPE); game->OnKeyb(KEYB_UP, A3D_ESCAPE);
    check(game->menu_depth < 0, "menu cancels");
    game->OnPadButton(10, true); game->OnPadButton(10, false);
    check(game->player.talk_box && game->show_keyb, "controller text keyboard opens");
    for (const char* text="\\this.consoleWorked=123"; *text; ++text) game->OnKeyb(KEYB_CHAR,*text);
    game->OnPadButton(3,true); game->OnPadButton(3,false);
    js("if(akAPI_This.consoleWorked!==123)throw Error('controller script console')");
    game->OnKeyb(KEYB_DOWN, A3D_ESCAPE); game->OnKeyb(KEYB_UP, A3D_ESCAPE);
    game->OnPadButton(10,true); game->OnPadButton(10,false);
    js("ak.onSay(function(text){this.controllerSaid=text;return false})");
    for (const char* text="veto"; *text; ++text) game->OnKeyb(KEYB_CHAR,*text);
    const int talks_before = game->player.talks;
    game->OnPadButton(3,true); game->OnPadButton(3,false);
    check(game->player.talks==talks_before,"controller speech honors script veto");
    js("if(akAPI_This.controllerSaid!=='veto')throw Error('controller onSay');ak.onSay(null)");
    game->OnKeyb(KEYB_DOWN, A3D_ESCAPE); game->OnKeyb(KEYB_UP, A3D_ESCAPE);
    check(!game->player.talk_box, "chat cancels");
    // Main menu navigation: continue the current world, then select new game.
    // New-game request is consumed at a safe frame boundary by the launcher.
    game->main_menu = true; MainMenu_Show(); MainMenu_OnSize(960,544,8,8);
    MainMenu_OnPadButton(0,true); MainMenu_OnPadButton(0,false);
    check(!game->main_menu, "main menu Continue resumes current world");
    game->main_menu = true; MainMenu_Show();
    MainMenu_OnKeyb(KEYB_DOWN, A3D_DOWN); MainMenu_OnKeyb(KEYB_UP, A3D_DOWN);
    MainMenu_OnKeyb(KEYB_CHAR, '\n');
    check(port::take_new_game_request(), "main menu New Game requests safe world/runtime reset");
    MainMenu_OnKeyb(KEYB_DOWN, A3D_ESCAPE); // Reset highlight to Continue.
    for (int i = 0; i < 6; ++i) MainMenu_OnKeyb(KEYB_DOWN, A3D_DOWN);
    MainMenu_OnKeyb(KEYB_CHAR, '\n'); // EXIT? submenu
    MainMenu_OnKeyb(KEYB_CHAR, '\n'); // NO (formerly indexed game->menu_depth == -1)
    check(!port::should_exit(), "main menu exit confirmation cancels safely");
    game->main_menu = false;

    // Settings round-trip and corrupt-file rejection in a separate test file;
    // never overwrite the user's real preference file.
    const std::string original_config = GetConfPath();
    if (!original_config.empty()) {
        const std::string test_config = original_config + ".parity-test";
        FILE* existing = std::fopen(test_config.c_str(), "rb");
        if (existing) { std::fclose(existing); throw std::runtime_error("stale parity-test settings: inspect/remove before retry"); }
        struct ConfigRestore {
            std::string original, temporary;
            ~ConfigRestore() { port::set_config_path(original.c_str()); std::remove(temporary.c_str()); }
        } restore_config{original_config,test_config};
        Game::TalkMem saved_talks[4]; std::memcpy(saved_talks,game->talk_mem,sizeof(saved_talks));
        const bool old_perspective=game->perspective, old_blood=game->blood, old_mute=game->mute;
        port::set_config_path(test_config.c_str());
        game->perspective=false; game->blood=true; game->mute=false;
        game->main_menu=true; MainMenu_Show();
        auto pad = [](int button) { MainMenu_OnPadButton(button,true); MainMenu_OnPadButton(button,false); };
        for(int i=0;i<3;++i) pad(12); // VIDEO
        pad(0);
#ifndef __vita__
        pad(12); // Desktop fullscreen precedes perspective.
#endif
        pad(0); check(game->perspective,"controller menu toggles perspective");
        pad(12); pad(0); check(!game->blood,"controller menu toggles blood");
        MainMenu_Show(); for(int i=0;i<5;++i) pad(12); pad(0);
        check(game->mute,"controller menu toggles audio mute");
        game->main_menu=false;
        game->perspective=true; game->blood=false; game->mute=false;
        game->talk_mem[0].len=5; std::memcpy(game->talk_mem[0].buf,"hello",6);
        WriteConf(game);
        game->perspective=false; game->blood=true; game->mute=true; game->talk_mem[0].len=0;
        ReadConf(game);
        check(game->perspective && !game->blood && !game->mute && game->talk_mem[0].len==5, "settings persist and restore");
        game->blood=true; WriteConf(game); game->blood=false; ReadConf(game);
        check(game->blood,"settings atomically replace an existing file");
        Game::TalkMem loaded_talks[4]; std::memcpy(loaded_talks,game->talk_mem,sizeof(loaded_talks));
        // Create an actually truncated fixture, even on Vita3K versions whose
        // HLE open-mode translator ignores O_TRUNC for existing files.
        check(std::remove(test_config.c_str())==0,"replace test fixture");
        FILE* corrupt=std::fopen(test_config.c_str(),"wb"); check(corrupt!=nullptr,"test config writable");
        std::fputc(255,corrupt); std::fclose(corrupt);
        ReadConf(game);
        check(game->perspective && game->blood && !game->mute && std::memcmp(loaded_talks,game->talk_mem,sizeof(loaded_talks))==0,"truncated config leaves settings unchanged");
        game->perspective=old_perspective; game->blood=old_blood; game->mute=old_mute;
        std::memcpy(game->talk_mem,saved_talks,sizeof(saved_talks));
        AudioMute(old_mute);
    }

    // Create a world item using the real prototype and exercise pickup,
    // callback veto, equip/unequip, and drop (including world ownership).
    const int before_items = game->inventory.my_items;
    Item* sword = CreateItem();
    sword->proto = item_proto_lib + 3; // upstream Alpha Sword
    sword->count = 1; sword->purpose = Item::WORLD;
    sword->inst = CreateInst(world, sword, INST_USE_TREE | INST_VISIBLE | INST_VOLATILE, game->player.pos, 0, 77);
    check(sword->inst != nullptr, "world sword instance");
    js("ak.onItem(function(){return false})");
    check(!game->PickItem(sword) && game->inventory.my_items == before_items, "script veto preserves world item");
    js("ak.onItem(function(action){if(action==='P'.charCodeAt(0))return [42,'test sword'];return true;})");
    check(game->PickItem(sword), "world item picked up");
    const int sword_index = game->inventory.my_items - 1;
    check(game->inventory.my_item[sword_index].story_id == 42 &&
          std::string(game->inventory.my_item[sword_index].desc) == "test sword", "pickup callback mutation applied");
    game->show_inventory=true; game->inventory.focus=sword_index;
    game->OnPadButton(2,true); game->OnPadButton(2,false);
    check(game->player.req.weapon == WEAPON::REGULAR_SWORD && game->inventory.my_item[sword_index].in_use, "sword equips");
    game->player.SetActionNone(game->stamp);
    game->OnPadButton(0, true); game->OnPadButton(0, false);
    check(game->player.req.action == ACTION::ATTACK, "controller attack reaches animation");
    game->player.SetActionNone(game->stamp);
    game->OnPadButton(2,true); game->OnPadButton(2,false);
    check(!game->inventory.my_item[sword_index].in_use, "sword unequips");
    game->show_inventory=false;
    check(game->DropItem(sword_index) && game->inventory.my_items == before_items, "item drops back into world");
    js("ak.onItem(null)");

    // Exercise position/direction/camera native setters and the real physics
    // update, then drive melee damage against an existing loaded NPC.
    Character* enemy = player_head;
    while (enemy && (!enemy->enemy || enemy->HP <= 0 || !enemy->data)) enemy = enemy->next;
    check(enemy != nullptr, "map has combat NPC");
    const float old_pos[3] = {game->player.pos[0], game->player.pos[1], game->player.pos[2]};
    const float target[3] = {enemy->pos[0], enemy->pos[1], enemy->pos[2]};
    std::vector<AnsiCell> combat_frame(120 * 68);
    game->input.jump = false;
    game->player.HP = game->player.MAX_HP;
    game->player.SetActionNone(game->stamp);
    check(game->player.SetWeapon(WEAPON::REGULAR_SWORD), "combat sword sprite");
    const int hp_before = enemy->HP;
    bool damage = false;
    for (int frame = 0; frame < 90 && !damage; ++frame) {
        float player_pos[3] = {target[0], target[1] + 1.5F, target[2]};
        float enemy_pos[3] = {target[0], target[1], target[2]};
        SetPhysicsPos(game->physics, player_pos, nullptr);
        SetPhysicsPos(static_cast<Physics*>(enemy->data), enemy_pos, nullptr);
        SetPhysicsDir(game->physics, 0);
        if (game->player.req.action != ACTION::ATTACK) game->player.SetActionAttack(game->stamp);
        game->Render(game->stamp + 16667, combat_frame.data(), 120, 68);
        damage = enemy->HP < hp_before;
    }
    check(damage, "real melee attack damages loaded enemy");
    game->player.SetActionNone(game->stamp);
    check(game->player.SetWeapon(WEAPON::REGULAR_CROSSBOW),"crossbow sprite");
    game->player.shoot_stamp=0;
    game->OnPadButton(0,true); game->OnPadButton(0,false);
    game->Render(game->stamp + 16667, combat_frame.data(),120,68);
    check(game->player.shooting && game->player.shoot_stamp>0 && std::isfinite(game->player.shoot_to[2]),"controller fires real crossbow projectile");
    // Hit resolution must not depend on scene rasterization or replay count.
    game->player.shoot_target = enemy;
    std::copy_n(game->player.shoot_from,3,game->player.shoot_to);
    const auto projectile_time = game->player.shoot_stamp+100000;
    port::asciicker::advance_projectiles(projectile_time);
    check(!game->player.shooting && !game->player.shoot_target,"projectile resolves without a render");
    const int resolved_hp = enemy->HP;
    port::asciicker::advance_projectiles(projectile_time);
    check(enemy->HP == resolved_hp,"projectile damage is applied only once");
    float restore[3] = {old_pos[0], old_pos[1], old_pos[2]};
    SetPhysicsPos(game->physics, restore, nullptr);
    game->player.SetActionNone(game->stamp);
    game->player.SetWeapon(WEAPON::NONE);
    js("ak.setPos([0,15,247]);ak.setDir(90);ak.setYaw(17);ak.setAction(0);if(ak.getAction()!==0)throw Error('action');ak.setMount(1);if(ak.getMount()!==1)throw Error('mount');ak.setMount(0)");
    game->Render(game->stamp + 16667, combat_frame.data(), 120, 68);
    check(std::fabs(game->prev_yaw - 17) < 0.1, "script camera yaw applied");
    check(std::isfinite(game->player.dir), "script direction applied");
    js("let position=[];ak.getPos(position);if(Math.abs(position[0])>1||Math.abs(position[1]-15)>1)throw Error('position');if(!Number.isFinite(ak.getDir())||Math.abs(ak.getYaw()-17)>0.1)throw Error('direction/yaw')");

    port::Mixer mixer;
    mixer.samples[0].pcm = {1000,-1000,2000,-2000};
    mixer.samples[1].pcm = {32767,32767,32767,32767};
    mixer.samples[1].markers = {{1,2}};
    check(mixer.enqueue({-1,0,0,-1}), "ambient queued");
    int16_t pcm[8]{};
    mixer.render(pcm,4);
    check(pcm[0] == 1000 && pcm[4] == 1000, "ambient loops stereo");
    mixer.enqueue({0,1,65535,0}); mixer.render(pcm,2);
    check(pcm[0] == 32767 && pcm[2] == 2000, "marker playback and saturating mix");
    mixer.enqueue({-2,-1,0,-1}); mixer.render(pcm,4);
    check(std::all_of(std::begin(pcm),std::end(pcm),[](int16_t sample){return sample==0;}), "mute");
    mixer.reset();
    for (int i=0;i<128;++i) check(mixer.enqueue({-2,-1,0,-1}), "queue capacity");
    check(!mixer.enqueue({}) && mixer.dropped()==1, "bounded queue overflow observable");
    mixer.render(pcm,1);
    check(mixer.enqueue({-2,-1,32768,-1}), "queue wraps");
    std::string error;
    for (const auto* file : {"forest.ogg","footsteps.ogg"}) {
        port::Sample sample;
        check(port::load_sample(root+"/samples/"+file,sample,error),error.c_str());
        check(!sample.pcm.empty(), "actual Ogg decoded");
        if (std::strcmp(file,"footsteps.ogg")==0) check(!sample.markers.empty(), "material/foot cue markers loaded");
    }
    check(port::load_script(root+"/scripts/startup.js",error),error.c_str());
    port::log_stage("PARITY_OK ak limits inventory-pick-equip-drop melee-damage crossbow controller-console menus config mixer-ogg");
}
