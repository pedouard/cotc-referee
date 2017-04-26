#pragma GCC optimize "O3,omit-frame-pointer,inline"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define QMAX 23
#define RMAX 21

#define MAX_SHIP_RUM 100
#define MAX_SHIP_SPEED 2
#define REWARD_RUM 30
#define MINE_VISIB_RANGE 5
#define FIRE_DIST_MAX 10
#define LOW_DMG 25
#define HIGH_DMG 50
#define MINE_DMG 25
#define MINE_CD 5
#define CANNON_CD 2
#define NEAR_MINE_DMG 10

#define N_SHIPS_MAX 6
#define N_BARRELS_MAX 30
#define N_CBALLS_MAX 30
#define N_MINES_MAX 100

#define WAIT 0
#define FASTER 1
#define SLOWER 2
#define PORT 3
#define STARBOARD 4
#define FIRE 5
#define MINE 6

#define NOTHING 0
#define BARREL 1
#define CANNONBALL 2
#define SHIP 3
//#define MINE 6

#define DEPTH 5

#define ABS(x) ((x)<0 ? -(x) : (x))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

struct xyz {
    int x;
    int y;
    int z;
} typedef xyz_t;

struct qr {
    int q;
    int r;
} typedef qr_t;

struct ship {
    int id;
    xyz_t xyz;
    xyz_t bow;
    xyz_t stern;
    xyz_t rot;

    xyz_t rot_tmp;
    xyz_t xyz_tmp;
    xyz_t bow_tmp;
    xyz_t stern_tmp;

    int initial_rum;
    int rum;
    int speed;
    int owner;

    int alive;
    int cannon_cd;
    int mine_cd;

    int has_rotate;
} typedef ship_t;

struct barrel {
    int id;
    xyz_t xyz;
    int rum;
} typedef barrel_t;

struct cball {
    int id;
    xyz_t xyz;
    int source;
    int delay;
} typedef cball_t;

struct mine {
    int id;
    xyz_t xyz;
} typedef mine_t;

struct map {
    int objects[QMAX*RMAX];
    int ships[QMAX*RMAX];
    int cballs[DEPTH][QMAX*RMAX];
} typedef map_t;

struct ctx {
    int turn;

    int nships;
    int nbarrels;
    int ncballs;
    int nmines;

    ship_t ships[N_SHIPS_MAX];
    barrel_t barrels[N_BARRELS_MAX];
    cball_t cballs[N_CBALLS_MAX];
    mine_t mines[N_MINES_MAX];
    map_t map;
} typedef ctx_t;

struct order {
    int orders[N_SHIPS_MAX];
    qr_t qr[N_SHIPS_MAX];
} typedef order_t;

struct order_list {
    order_t orders[DEPTH];
} typedef order_list_t;

int collisions[N_SHIPS_MAX], tmp_int;
qr_t tmp_qr;

ctx_t ctx, ctx_future;
order_list_t *orders_list;

void inline xyz_to_qr(xyz_t *xyz, qr_t *qr) {
    // convert xyz to odd-r qr
    qr->q = xyz->x + ((xyz->y - (xyz->y&1)) >> 1);
    qr->r = xyz->y;
}

void inline add_xyz(xyz_t *a, xyz_t *b) {
    a->x += b->x;
    a->y += b->y;
    a->z += b->z;
}

void inline sub_xyz(xyz_t *a, xyz_t *b) {
    a->x -= b->x;
    a->y -= b->y;
    a->z -= b->z;
}

void inline mult_xyz(xyz_t *a, int b) {
    a->x *= b;
    a->y *= b;
    a->z *= b;
}

int inline equal_xyz(xyz_t *a, xyz_t *b) {
    //fprintf(stderr, "%d %d %d %d %d %d\n", a->x, b->x, a->y, b->y, a->z, b->z);
    return (a->x == b->x && a->y == b->y && a->z == b->z) ? 1 : 0;
}

void inline qr_to_xyz(qr_t qr, xyz_t *xyz) {
    // convert odd-r qr to xyz
    xyz->x = qr.q - ((qr.r - (qr.r&1)) >> 1);
    xyz->y = qr.r;
    xyz->z = -xyz->x - qr.r;
}

void inline print_xyz(xyz_t *xyz) {
    fprintf(stderr, "x:%d y:%d z:%d ", xyz->x, xyz->y, xyz->z);
}

int inline get_dist(xyz_t *a, xyz_t *b) {
    // xyz dist
    return (ABS(a->x - b->x) + ABS(a->y - b->y) + ABS(a->z - b->z)) >> 1;
}

void inline apply_port(xyz_t *rot) {
    tmp_int = - rot->z;
    rot->z = - rot->y;
    rot->y = - rot->x;
    rot->x = tmp_int;
}

void inline apply_starboard(xyz_t *rot) {
    tmp_int = - rot->z;
    rot->z = - rot->x;
    rot->x = - rot->y;
    rot->y = tmp_int;
}

void inline place_bow_stern(ship_t *ship) {
    ship->stern = ship->xyz;
    ship->bow = ship->xyz;
    add_xyz(&(ship->bow), &(ship->rot));
    sub_xyz(&(ship->stern), &(ship->rot));
}

void inline get_direction(int rot, xyz_t *a) {
    a->x = 1; a->y = 0; a->z = -1;
    int i;
    for (i=0; i<rot; i++) {
        apply_port(a);
    }
}

int inline is_in_map(xyz_t *xyz) {
    xyz_to_qr(xyz, &tmp_qr);
    return (tmp_qr.q >= 0 && tmp_qr.q < QMAX && tmp_qr.r >= 0 && tmp_qr.r < RMAX) ? 1 : 0;
}

int inline map_get(int *map, xyz_t *xyz) {
    xyz_to_qr(xyz, &tmp_qr);

    if (is_in_map(xyz) == 0) {
        return NOTHING;
    }

    return map[tmp_qr.q * RMAX + tmp_qr.r];
}

void inline map_set(int *map, xyz_t *xyz, int value) {
    xyz_to_qr(xyz, &tmp_qr);
    map[tmp_qr.q * RMAX + tmp_qr.r] = value;
}

void inline print_map(map_t *map) {
    int obj;
    for (int x=0; x<RMAX; x++) {
        if (x & 1) fprintf(stderr, " ");
        for (int y=0; y<QMAX; y++) {
            obj = map->objects[y * RMAX + x] + map->ships[y * RMAX + x];
            fprintf(stderr, "%d ", obj);
        }
        fprintf(stderr, "\n");
    }
}

void inline do_action(int cmd, qr_t qr, int sid) {
    switch(cmd) {
        case WAIT:
            printf("WAIT WAIT\n");
            break;
        case PORT:
            printf("PORT\n");
            break;
        case STARBOARD:
            printf("STARBOARD\n");
            break;
        case SLOWER:
            printf("SLOWER\n");
            break;
        case FASTER:
            printf("FASTER\n");
            break;
        case MINE:
            printf("MINE MINE\n");
            break;
        case FIRE:
            printf("FIRE %d %d FIRE\n", qr.q, qr.r);
            break;
        default:
            break;
    }
}

void inline reset_map(map_t *map) {
    memset(&(map->objects), 0 , sizeof(map->objects));
    memset(&(map->ships), 0 , sizeof(map->ships));
    //memset(&(ctx.map.cballs), 0 , sizeof(ctx.map.cballs));
}

void inline copy_ctx() {
    int i;
    xyz_t xyz;
    ctx_future.turn = ctx.turn;
    ctx_future.nships = ctx.nships;
    ctx_future.nbarrels = ctx.nbarrels;
    ctx_future.ncballs = ctx.ncballs;
    ctx_future.nmines = ctx.nmines;

    reset_map(&(ctx_future.map));

    for (i=0; i<ctx_future.nships; i++) {
        ctx_future.ships[i] = ctx.ships[i];
        map_set(ctx_future.map.ships, &(ctx_future.ships[i].bow), i+10);
        map_set(ctx_future.map.ships, &(ctx_future.ships[i].xyz), i+10);
        map_set(ctx_future.map.ships, &(ctx_future.ships[i].stern), i+10);
    }

    for (i=0; i<ctx.nbarrels; i++) {
        ctx_future.barrels[i] = ctx.barrels[i];
        xyz = ctx_future.barrels[i].xyz;
        map_set(ctx_future.map.objects, &xyz, ctx.barrels[i].rum);
    }

    for (i=0; i<ctx.ncballs; i++) {
        ctx_future.cballs[i] = ctx.cballs[i];
        xyz = ctx_future.cballs[i].xyz;
    }

    for (i=0; i<ctx.nmines; i++) {
        ctx_future.mines[i] = ctx.mines[i];
        xyz = ctx_future.mines[i].xyz;
        map_set(ctx_future.map.objects, &xyz, MINE);
    }
}

int inline ship_will_collide(ship_t *ship) {
    int sid;
    ship_t *ship_;

    for(sid=0; sid<ctx.nships; sid++) {
        ship_ = &(ctx_future.ships[sid]);

        if (ship_->id == ship->id || ship_->alive == 0) {
            continue;
        }

        if (equal_xyz(&(ship->bow), &(ship_->bow)) \
                || equal_xyz(&(ship->bow), &(ship_->xyz)) \
                || equal_xyz(&(ship->bow), &(ship_->stern)) \

                || equal_xyz(&(ship->xyz), &(ship_->bow)) \
                || equal_xyz(&(ship->xyz), &(ship_->xyz)) \
                || equal_xyz(&(ship->xyz), &(ship_->stern)) \

                || equal_xyz(&(ship->stern), &(ship_->bow)) \
                || equal_xyz(&(ship->stern), &(ship_->xyz)) \
                || equal_xyz(&(ship->stern), &(ship_->stern)) \
                ) {
            return 1;
        }
    }

    return 0;
}

int inline bow_will_collide(ship_t *ship) {
    int sid;
    ship_t *ship_;

    for(sid=0; sid<ctx.nships; sid++) {
        ship_ = &(ctx_future.ships[sid]);

        if (ship_->id == ship->id || ship_->alive == 0) {
            continue;
        }

        if (equal_xyz(&(ship->bow_tmp), &(ship_->bow_tmp)) \
                || equal_xyz(&(ship->bow_tmp), &(ship_->xyz_tmp)) \
                || equal_xyz(&(ship->bow_tmp), &(ship_->stern_tmp))
                ) {
            //fprintf(stderr, "COLLISION %d %d\n", ship->id, ship_->id);
            return 1;
        }
    }

    return 0;
}

void inline apply_damage(ship_t *ship, int dmg) {
    ship->rum -= dmg;
    if (ship->rum <= 0) {
        ship->rum = 0;
        ship->alive = 0;
    }
}

void inline apply_heal(ship_t *ship, int rum) {
    if (ship->alive == 0) {
        return;
    }

    ship->rum += rum;
    if (ship->rum > MAX_SHIP_RUM) {
        ship->rum = MAX_SHIP_RUM;
    }
}

void inline explode(xyz_t *xyz) {
    int sid, d;
    ship_t *ship;

    for(sid=0; sid<ctx_future.nships; sid++) {
        ship = &(ctx_future.ships[sid]);

        if (ship->alive == 0) {
            continue;
        }

        // opti
        d = get_dist(&(ship->xyz), xyz);
        if (d > 2) {
            continue;
        }

        if (d <= 1 \
                || get_dist(&(ship->bow), xyz) <= 1 \
                || get_dist(&(ship->stern), xyz) <= 1) {
            apply_damage(ship, MINE_DMG);
        }
    }

    map_set(ctx_future.map.objects, xyz, 0);
}

void inline handle_object_collisions(int turn, ship_t *ship) {
    int value;

    if (ship->alive == 0) {
        return;
    }

    value = map_get(ctx_future.map.objects, &(ship->bow));
    if (value >= 10){
        apply_heal(ship, value);
        map_set(ctx_future.map.objects, &(ship->bow), NOTHING);
    } else if (value == MINE){
        explode(&(ship->bow));
    }

    value = map_get(ctx_future.map.objects, &(ship->xyz));
    if (value >= 10){
        apply_heal(ship, value);
        map_set(ctx_future.map.objects, &(ship->xyz), NOTHING);
    } else if (value == MINE){
        explode(&(ship->xyz));
    }

    value = map_get(ctx_future.map.objects, &(ship->stern));
    if (value >= 10){
        apply_heal(ship, value);
        map_set(ctx_future.map.objects, &(ship->stern), NOTHING);
    } else if (value == MINE){
        explode(&(ship->stern));
    }
}

void run();
void reset();
void predict_future(int target_owner);

int main() {
    int seed = time(NULL);
    srand(seed);

    while (1) {
        reset_map(&(ctx.map));
        reset();
        run();
    }

    return 0;
}

void reset() {
    int i;

    ctx.nships = 0;
    ctx.nbarrels = 0;
    ctx.ncballs = 0;
    ctx.nmines = 0;

    int tmp, entity_count;
    scanf("%d", &tmp);

    scanf("%d", &entity_count);
    for (int i = 0; i < entity_count; i++) {
        int entity_id, x, y, arg1, arg2, arg3, arg4;
        char entity_type[11];
        xyz_t xyz; qr_t qr;

        scanf("%d%s%d%d%d%d%d%d", &entity_id, entity_type, &x, &y, &arg1, &arg2, &arg3, &arg4);
        qr = (qr_t) { .q = x, .r = y };
        qr_to_xyz(qr, &xyz);

        switch(entity_type[0]) {

            case 'S': ;// Ship
                xyz_t rot;
                get_direction(arg1, &rot);
                ctx.ships[ctx.nships] = (ship_t) { .id=entity_id, .xyz=xyz, .rot=rot, .speed=arg2, .rum=arg3, .owner=arg4, .alive=1 };
                place_bow_stern(&ctx.ships[ctx.nships]);
                map_set(ctx.map.ships, &xyz, ctx.nships+10);
                map_set(ctx.map.ships, &(ctx.ships[ctx.nships].bow), ctx.nships+10);
                map_set(ctx.map.ships, &(ctx.ships[ctx.nships].stern), ctx.nships+10);
                ctx.nships += 1;
                break;
            case 'B': ;// Barrel
                ctx.barrels[ctx.nbarrels] = (barrel_t) { .id=entity_id, .xyz=xyz, .rum=arg1 };
                map_set(ctx.map.objects, &xyz, arg1);
                ctx.nbarrels += 1;
                break;
            case 'C': ;// Cannonball
                ctx.cballs[ctx.ncballs] = (cball_t) { .id=entity_id, .xyz=xyz, .source=arg1, .delay=arg2 };
                map_set(ctx.map.objects, &xyz, arg2);
                ctx.ncballs += 1;
                break;
            case 'M': ;// Mines
                ctx.mines[ctx.nmines] = (mine_t) { .id=entity_id, .xyz=xyz };
                map_set(ctx.map.objects, &xyz, MINE);
                ctx.nmines += 1;
                break;
            default:
                break;
        }
    }

    for (i=0; i<ctx.nships; i++) {

        if (ctx.ships[i].mine_cd > 0) {
            ctx.ships[i].mine_cd--;
        }
        if (ctx.ships[i].cannon_cd > 0) {
            ctx.ships[i].cannon_cd--;
        }
    }

    return;
}

void run() {
    int i, ship_id, turn;
    order_list_t cmd;

    // Set command for all ships: FIRE at yourself
    for (turn=0; turn < DEPTH; turn++) {
        for (ship_id=0; ship_id < ctx.nships; ship_id++) {
            cmd.orders[turn].orders[ship_id] = FIRE;
            xyz_to_qr( &(ctx.ships[ship_id].xyz), &(cmd.orders[turn].qr[ship_id]) );
        }
    }

    // Commands used in the simulations are stored in "orders_list"
    orders_list = &cmd;
    // Predict the next DEPTH turns using the specified commands
    predict_future(1);

    // Print in stdout the actions for the current turn
    for (i=0; i < ctx.nships; i++) {
        if (ctx.ships[i].owner != 1) {
            continue;
        }
        do_action(orders_list->orders[0].orders[i], orders_list->orders[0].qr[i], i);
    }

    ctx.turn++;
    return;
}

void inline apply_actions(int turn, order_t *orders, int target_owner);
void inline move_forward(int turn);
void inline rotate_ships(int turn);
void inline explode_things(int turn);

void predict_future(int target_owner) {
    int i, turn, sid;
    ship_t *ship;
    xyz_t tmp;
    order_t *orders;
    copy_ctx();

    for(turn=0; turn<DEPTH; turn++) {
        orders = &(orders_list->orders[turn]);

        for (i=0; i<ctx_future.nships; i++) {
            ctx_future.ships[i].initial_rum = ctx_future.ships[i].rum;
            apply_damage(&(ctx_future.ships[i]), 1);
        }

        apply_actions(turn, orders, target_owner);

        for (i=0; i<ctx_future.nships; i++) {
            ship = &(ctx_future.ships[i]);
            map_set(ctx_future.map.ships, &(ship->xyz), 0);
            map_set(ctx_future.map.ships, &(ship->bow), 0);
            map_set(ctx_future.map.ships, &(ship->stern), 0);
        }

        move_forward(turn);
        rotate_ships(turn);

        for (i=0; i<ctx_future.nships; i++) {
            ship = &(ctx_future.ships[i]);

            if(ship->alive == 0) {
                continue;
            }

            map_set(ctx_future.map.ships, &(ship->xyz), 10+i);
            map_set(ctx_future.map.ships, &(ship->bow), 10+i);
            map_set(ctx_future.map.ships, &(ship->stern), 10+i);
        }

        explode_things(turn);

        for (i=0; i<ctx_future.nships; i++) {
            if (ctx_future.ships[i].rum <= 0) {
                map_set(ctx_future.map.objects, &(ctx_future.ships[i].xyz), \
                        MIN(REWARD_RUM, ctx_future.ships[i].initial_rum));
            }
        }
    }

    return;
}

void inline explode_things(int turn) {
    int value, i;
    xyz_t xyz_tmp;
    ship_t *ship;

    for (i=0; i<ctx_future.ncballs; i++) {
        if (ctx_future.cballs[i].delay - turn != 1){
            continue;
        }

        xyz_tmp = ctx_future.cballs[i].xyz;

        value = map_get(ctx_future.map.objects, &xyz_tmp);
        if (value >= 10) {
            // explode barrel
            map_set(ctx_future.map.objects, &xyz_tmp, 0);
            continue;
        } else if (value == MINE) {
            // explode mine
            explode(&xyz_tmp);
            continue;
        }

        value = map_get(ctx_future.map.ships, &xyz_tmp);
        if (value > 0) {
            // explode ship
            ship = &(ctx_future.ships[value-10]);

            if (equal_xyz(&xyz_tmp, &(ship->xyz))) {
                apply_damage(ship, HIGH_DMG);
            } else {
                apply_damage(ship, LOW_DMG);
            }
            continue;
        }
    }
}

void inline apply_actions(int turn, order_t *orders, int target_owner) {
    int sid;
    ship_t *ship;
    xyz_t tmp;

    /* Apply Actions */
    for(sid=0; sid<ctx.nships; sid++) {
        ship = &(ctx_future.ships[sid]);

        if (ship->alive == 0) {
            continue;
        }

        if (ship->mine_cd > 0) {
            ship->mine_cd--;
        }
        if (ship->cannon_cd > 0) {
            ship->cannon_cd--;
        }

        ship->rot_tmp = ship->rot;
        ship->has_rotate = 0;

        switch(orders->orders[sid]) {
            case WAIT:
                break;

            case FASTER:
                if (ship->speed < MAX_SHIP_SPEED) {
                    ship->speed++;
                }
                break;

            case SLOWER:
                if (ship->speed > 0) {
                    ship->speed--;
                }
                break;

            case PORT:
                apply_port(&(ship->rot_tmp));
                ship->has_rotate = 1;
                break;

            case STARBOARD:
                apply_starboard(&(ship->rot_tmp));
                ship->has_rotate = 1;
                break;

            case MINE:
                if (ship->mine_cd != 0) {
                    break;
                }
                ship->mine_cd = MINE_CD;
                xyz_t target = ship->stern;
                sub_xyz(&target, &ship->rot);

                if (is_in_map(&target)){

                    if (map_get(ctx_future.map.objects, &target) == 0 && map_get(ctx_future.map.ships, &target) == 0){
                        map_set(ctx_future.map.objects, &target, MINE);
                        ship->mine_cd = MINE_CD;
                    }
                }
                break;

            case FIRE:
                if (ship->cannon_cd != 0) {
                    break;
                }
                ship->cannon_cd = CANNON_CD;
                qr_t qr = orders->qr[sid];
                xyz_t xyz_tmp;
                qr_to_xyz(qr, &xyz_tmp);
                int distance = get_dist(&xyz_tmp, &(ship->xyz));
                if (is_in_map(&xyz_tmp) && distance < FIRE_DIST_MAX) {
                    int traveltime = (int) (2 + round(get_dist(&xyz_tmp, &(ship->bow)) / 3.0));
                    if (turn + traveltime < DEPTH) {
                        ctx_future.cballs[ctx_future.ncballs] = (cball_t) { .id=666, .xyz=xyz_tmp, .source=ship->id, .delay=traveltime };
                        ctx_future.ncballs += 1;
                    }
                }
                break;

            default:
                break;
        }
    }
}

void inline move_forward(int turn) {
    int i, sid;
    ship_t *ship;
    xyz_t tmp;

    for (i=1; i <= MAX_SHIP_SPEED; i++) {

        for(sid=0; sid<ctx.nships; sid++) {
            ship = &(ctx_future.ships[sid]);

            if (ship->alive == 0) {
                continue;
            }

            ship->xyz_tmp = ship->xyz;
            ship->bow_tmp = ship->bow;
            ship->stern_tmp = ship->stern;

            if (i > ship->speed) {
                continue;
            }

            if (is_in_map(&(ship->bow))) {
                xyz_t bow = ship->bow_tmp;
                add_xyz(&bow, &(ship->rot));

                ship->stern_tmp = ship->xyz_tmp;
                ship->xyz_tmp = ship->bow_tmp;
                ship->bow_tmp = bow;

            } else {
                // Stop ship!
                ship->speed = 0;
            }
        }

        // Check ship and obstacles collision
        memset(collisions, 0, sizeof(collisions));
        int is_collision = 1;
        while (is_collision) {
            is_collision = 0;

            for(sid=0; sid<ctx.nships; sid++) {
                ship = &(ctx_future.ships[sid]);

                if (ship->alive == 0) {
                    continue;
                }

                if (i <= ship->speed && bow_will_collide(ship)) {
                    collisions[sid] = 1;
                }
            }

            for(sid=0; sid<ctx.nships; sid++) {
                if (collisions[sid] == 0) {
                    continue;
                }

                ship = &(ctx_future.ships[sid]);
                // Revert last move
                ship->stern_tmp = ship->stern;
                ship->xyz_tmp = ship->xyz;
                ship->bow_tmp = ship->bow;

                // Stop ships
                ship->speed = 0;
                is_collision = 1;
            }

            memset(collisions, 0, sizeof(collisions));
        }

        for(sid=0; sid<ctx.nships; sid++) {
            /*if (ship->alive == 0) {
                continue;
            }*/

            ship = &(ctx_future.ships[sid]);
            ship->stern = ship->stern_tmp;
            ship->xyz = ship->xyz_tmp;
            ship->bow = ship->bow_tmp;
        }

        for(sid=0; sid<ctx.nships; sid++) {
            ship = &(ctx_future.ships[sid]);

            if (ship->alive == 0) {
                continue;
            }

            handle_object_collisions(turn, ship);
        }
    }
}

void inline rotate_ships(int turn) {
    int sid;
    ship_t *ship;
    xyz_t tmp;

    // Rotate
    for(sid=0; sid<ctx.nships; sid++) {
        ship = &(ctx_future.ships[sid]);

        if (ship->has_rotate == 0 || ship->alive == 0) {
            continue;
        }

        tmp = ship->rot;
        ship->rot = ship->rot_tmp;
        ship->rot_tmp = tmp;
        place_bow_stern(ship);

    }

    // Check collisions
    memset(collisions, 0, sizeof(collisions));
    int is_collision = 1;

    while (is_collision) {
        is_collision = 0;
        for(sid=0; sid<ctx.nships; sid++) {
            ship = &(ctx_future.ships[sid]);
            if (ship->alive == 0) {
                continue;
            }

            if (ship->has_rotate == 1 && ship_will_collide(ship)) {
                collisions[sid] = 1;
            }
        }

        for(sid=0; sid<ctx.nships; sid++) {
            if (collisions[sid] == 0) {
                continue;
            }

            ship = &(ctx_future.ships[sid]);
            if (ship->alive == 0) {
                continue;
            }

            // revert rotation
            ship->rot = ship->rot_tmp;
            place_bow_stern(ship);
            // Stop ships
            ship->speed = 0;
            is_collision = 1;
        }

        for(sid=0; sid<ctx.nships; sid++) {
            ship = &(ctx_future.ships[sid]);
            handle_object_collisions(turn, ship);
        }

        memset(collisions, 0, sizeof(collisions));
    }
}
