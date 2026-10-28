"""C150: assemble two local demo profiles from an already green, reviewed CI pair."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

SOURCE = 'b8b444d6c858f27a1fdde0d93b25a318cafc6937'
CI = 40709459511
HASHES = {
    'CryMP-Client64.exe': 'ae9ee3fa059ea31557f644eaaafe990876f5e6beda93c46c80329f63a2ea889e',
    'CryMP-Server64.exe': '8fd34b5399b1bb0cba0b5c610bc29d37141776993bff9c9535462e7644404d70',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=True) + '\n', encoding='utf-8')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', required=True, type=Path)
    args = ap.parse_args()
    repo = Path(__file__).resolve().parent.parent
    output = args.output.resolve()
    if output.exists():
        raise SystemExit('Output already exists; no owner files overwritten')
    binaries = repo / 'build/ci-b8b444d6'
    for name, expected in HASHES.items():
        if digest(binaries / name) != expected:
            raise SystemExit('Exact CI artifact mismatch: ' + name)
    canonical = repo / 'build/c119-canonical-data'
    for kind in json.loads((canonical / 'Steelmill.manifest.json').read_text())['files'].values():
        if digest(canonical / kind['name']) != kind['sha256']:
            raise SystemExit('Canonical Steelmill data mismatch')
    shore = repo / 'build/c149-route-b8b444d6-v1/candidate'
    if digest(shore / 'Shore.navmesh') != 'e2235993ce72eb4549788ad7bd1a171ab415c8e8b4c6da69fdb49ed941ef30de':
        raise SystemExit('Audited Shore data mismatch')
    base = json.loads((repo / 'build/c127-smoke-4f3a1d72/recipe.json').read_text())
    output.mkdir(parents=True)
    for map_name, mode, level, port, maximum, retained, polys, data_root, label in [
        ('Steelmill', 'InstantAction', 'multiplayer/ia/Steelmill', 46160, 8, 8, 9333, canonical, 'S150M'),
        ('Shore', 'PowerStruggle', 'multiplayer/ps/Shore', 46161, 2, 1, 46614, shore, 'S150S'),
    ]:
        root = output / map_name
        root.mkdir()
        (root / 'Data').mkdir()
        for name in HASHES:
            shutil.copy2(binaries / name, root / name)
        shutil.copy2(binaries / 'sha256.txt', root / 'ci-sha256.txt')
        shutil.copy2(repo / 'Tools/Launch-WarsBotsShowcase.ps1', root / 'Run.ps1')
        for suffix in ['navmesh', 'links.txt', 'spots.txt', 'manifest.json']:
            shutil.copy2(data_root / (map_name + '.' + suffix), root / 'Data' / (map_name + '.' + suffix))
        if map_name == 'Shore':
            manifest_path = root / 'Data/Shore.manifest.json'
            manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
            manifest['source'] = 'C149 b8b444d6: audited upper exit to shore only; partial map, no default promotion'
            write_json(manifest_path, manifest)
        flags = dict(base)
        flags.update(wb_maxbots=maximum, wb_bot_navmesh=1, wb_bot_jump_links=1,
                     wb_log=2, wb_move_trace=1, wb_ps_capture=int(map_name == 'Shore'),
                     wb_ps_buy=int(map_name == 'Shore'), wb_ps_events=int(map_name == 'Shore'),
                     wb_ps_doors=0, wb_ps_vehicles=0)
        # Promoted defaults are checked, not overridden.
        for name in ['wb_bot_corners', 'wb_bot_jump_sanity']:
            flags.pop(name, None)
        critical = {name: flags[name] for name in [
            'wb_probe_local_test', 'wb_maxbots', 'wb_bot_navmesh', 'wb_bot_jump_links',
            'wb_net_ext', 'wb_ps_capture', 'wb_ps_buy', 'wb_ps_doors', 'wb_ps_vehicles',
            'wb_demo_record', 'wb_demo_hz', 'wb_move_trace', 'wb_log']}
        critical.update(wb_bot_corners=1, wb_bot_jump_sanity=1)
        write_json(root / 'preset.json', dict(map=map_name, mode=mode, level=level,
                   port=port, retained_bots=retained, polys=polys, label=label,
                   flags=flags, critical=critical))
        write_json(root / 'build.json', dict(source=SOURCE, ci=CI, ci_success=True,
                   code_review='b3182ff90373ad00731e75ea7d44df82a15c15d2',
                   runtime_audit='114257044f743b88520bf60648486dc5debe9961',
                   artifacts=HASHES, local_only=True,
                   validation='Steelmill canonical C119 data; Shore upper route C149 PASS; no full-map promotion'))
        (root / 'game-dir.txt').write_text('E:/Steam/steamapps/common/Crysis\n', encoding='utf-8')
        for name, switch in [('START.cmd', ''), ('CHECK.cmd', ' -PrepareOnly'), ('RESTORE.cmd', ' -RecoverLast')]:
            cmd = '@echo off\r\ncd /d "%~dp0"\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run.ps1"' + switch + '\r\npause\r\n'
            (root / name).write_bytes(cmd.encode('ascii'))
        for name in ['LICENSE.txt', 'LICENSE', 'LICENSE.md']:
            source = repo / 'build/ds076-code' / name
            if source.is_file():
                shutil.copy2(source, root / name)
                break
        licenses = root / 'Licenses'
        licenses.mkdir()
        crytek = Path('C:/Users/Magic8/OneDrive/Документы/ChatGPT/Crysis wars bot/LICENSE.txt')
        if crytek.is_file():
            shutil.copy2(crytek, licenses / 'Crytek-SDK-LICENSE.txt')
        shutil.copy2(repo / 'build/ds076-code/ThirdParty/recastnavigation/License.txt',
                     licenses / 'Recast-LICENSE.txt')
        intro = ('Steelmill: Instant Action, 8 ботов Normal. Можно играть или наблюдать за их боем.\n'
                 'В режиме наблюдателя: F6 — следующий бот, F7 — предыдущий, F1 — вид от первого лица.\n'
                 if map_name == 'Steelmill' else
                 'Shore: Power Struggle, один бот US (WarsBot_02). Это показ проверенного маршрута верх подлодки → море → берег.\n'
                 'Бот начинает двигаться сразу после готовности сервера; путь занимает примерно 1,5 минуты.\n'
                 'Выбирай US, чтобы пройти путь рядом с ним, или NK, чтобы встретить его на берегу.\n'
                 'Команда наблюдения wb_watch работает только в Steelmill, не в Power Struggle.\n'
                 'Нижние спавны, другие маршруты и полноценный матч Shore ещё не подтверждены; профиль намеренно ограничен.\n')
        readme = ('WARSBOTS — ЛОКАЛЬНЫЙ ПОКАЗ НА ОДНОМ ПК\n\n' + intro + '\n'
                  '1. Закрой предыдущую игровую сессию.\n'
                  '2. Дважды нажми START.cmd. Сервер и клиент откроются автоматически после проверки карты.\n'
                  '3. Окно запуска оставь открытым. Ограничения времени игры нет.\n'
                  '4. Выйди из игры обычным способом. Дождись Archived and restored в окне запуска.\n'
                  '5. Теперь можно запускать другую карту.\n\n'
                  'F8 — поставить отметку в журнале. Консоль ~: r_displayInfo 1 показывает FPS, r_displayInfo 0 скрывает.\n'
                  'CHECK.cmd проверяет файлы и команды без запуска игры.\n'
                  'Если окно запуска закрыли раньше игры: закрой игру, затем запусти RESTORE.cmd.\n'
                  'Если запущена другая игра, она остаётся нетронутой; восстановление ждёт её завершения.\n\n'
                  'Требуется установленный Crysis 2007. Путь указан в game-dir.txt; при необходимости измени его.\n'
                  'Пакет содержит проверенную пару CI, а не файлы самой игры.\n'
                  'Сборка b8b444d6, CI 40709459511, клиент/сервер одной версии. Локальная сборка не выполнялась.\n'
                  'Навигационные данные устанавливаются временно; прежние память и карты восстанавливаются после выхода.\n'
                  'Журналы, демо и снимки находятся в Sessions внутри этой папки.\n'
                  'Подготовка пакета: хеши/команды/AST/восстановление. Новый полный игровой прогон этих двух пресетов не запускался.\n')
        (root / 'README-RU.txt').write_text(readme, encoding='utf-8-sig')
        files = []
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.name not in ['game-dir.txt', 'README-RU.txt']:
                files.append(dict(name=path.relative_to(root).as_posix(), sha256=digest(path)))
        write_json(root / 'package-manifest.json', dict(source=SOURCE, files=files))
    (output / 'START-HERE.txt').write_text(
        'Сначала Steelmill/START.cmd — полноценный бой с 8 ботами.\n'
        'После выхода из игры и Archived and restored — Shore/START.cmd для показа маршрута из подлодки.\n'
        'Обе папки используют одну проверенную CI-версию b8b444d6, но разные режимы и данные карт.\n'
        'Подробности: README-RU.txt в каждой папке. Одновременно запускай одну карту.\n', encoding='utf-8-sig')
    print(output)


if __name__ == '__main__':
    main()
