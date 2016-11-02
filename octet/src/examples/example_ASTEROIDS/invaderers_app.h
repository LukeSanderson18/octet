﻿////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2014
//
// Modular Framework for OpenGLES2 rendering on multiple platforms.
//
// invaderer example: simple game with sprites and sounds
//
// Level: 1
//
// Demonstrates:
//   Basic framework app
//   Shaders
//   Basic Matrices
//   Simple game mechanics
//   Texture loaded from GIF file
//   Audio
//

#include <fstream> // functions for handling input output

namespace octet {

	class sprite {
		// where is our sprite (overkill for a 2D game!)

		// half the width of the sprite
		float halfWidth;

		// half the height of the sprite
		float halfHeight;

		// what texture is on our sprite
		int texture;

		// true if this sprite is enabled.
		bool enabled;



	public:

		mat4t modelToWorld;


		sprite() {
			texture = 0;
			enabled = true;
		}

		float xDir;						//for the asteroids' random X velocity at start
		float yDir;						//for the asteroids' random Y velocity at start

		float playerRotation = 0;		//keeping track of the player's rotation	
		float missileRotation = 0;		//keeping track of the bullet's and asteroid's rotation

		int asteroidSize = 0;			//1 for big, 2 for small, 3 for tiny.

		vec3 velocity;

		float modelScale;

		float missileTimer;

		float explosion_speed;


		void init(int _texture, float x, float y, float w, float h) {
			modelToWorld.loadIdentity();
			modelToWorld.translate(x, y, 0);
			halfWidth = w * 0.5f;
			halfHeight = h * 0.5f;
			texture = _texture;
			enabled = true;
		}

		void render(texture_shader &shader, mat4t &cameraToWorld) {
			// invisible sprite... used for gameplay.
			if (!texture) return;

			// build a projection matrix: model -> world -> camera -> projection
			// the projection space is the cube -1 <= x/w, y/w, z/w <= 1
			mat4t modelToProjection = mat4t::build_projection_matrix(modelToWorld, cameraToWorld);

			// set up opengl to draw textured triangles using sampler 0 (GL_TEXTURE0)
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);

			// use "old skool" rendering
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			shader.render(modelToProjection, 0);

			// this is an array of the positions of the corners of the sprite in 3D
			// a straight "float" here means this array is being generated here at runtime.
			float vertices[] = {
			  -halfWidth, -halfHeight, 0,
			   halfWidth, -halfHeight, 0,
			   halfWidth,  halfHeight, 0,
			  -halfWidth,  halfHeight, 0,
			};

			// attribute_pos (=0) is position of each corner
			// each corner has 3 floats (x, y, z)
			// there is no gap between the 3 floats and hence the stride is 3*sizeof(float)
			glVertexAttribPointer(attribute_pos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)vertices);
			glEnableVertexAttribArray(attribute_pos);

			// this is an array of the positions of the corners of the texture in 2D
			static const float uvs[] = {
			   0,  0,
			   1,  0,
			   1,  1,
			   0,  1,
			};

			// attribute_uv is position in the texture of each corner
			// each corner (vertex) has 2 floats (x, y)
			// there is no gap between the 2 floats and hence the stride is 2*sizeof(float)
			glVertexAttribPointer(attribute_uv, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)uvs);
			glEnableVertexAttribArray(attribute_uv);

			// finally, draw the sprite (4 vertices)
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}

		// move the object
		void translate(float x, float y) {
			modelToWorld.translate(x, y, 0);
		}

		//rotate the object
		void rotate(float x) {
			modelToWorld.rotateZ(x);
			playerRotation += x;
		}

		void scale(float x)
		{
			modelToWorld.scale(x, x, 0);
			modelScale += x;
		}

		// position the object relative to another.
		void set_relative(sprite &rhs, float x, float y) {
			modelToWorld = rhs.modelToWorld;
			modelToWorld.translate(x, y, 0);
		}

		// return true if this sprite collides with another.
		// note the "const"s which say we do not modify either sprite
		bool collides_with(const sprite &rhs) const {
			float dx = rhs.modelToWorld[3][0] - modelToWorld[3][0];
			float dy = rhs.modelToWorld[3][1] - modelToWorld[3][1];

			// both distances have to be under the sum of the halfwidths
			// for a collision
			return
				(fabsf(dx) < halfWidth + rhs.halfWidth) &&
				(fabsf(dy) < halfHeight + rhs.halfHeight)
				;
		}

		/*bool is_above(const sprite &rhs, float margin) const {
			float dx = rhs.modelToWorld[3][0] - modelToWorld[3][0];

			return
				(fabsf(dx) < halfWidth + margin)
				;
		}
		*/

		bool &is_enabled() {
			return enabled;
		}
	};

	class invaderers_app : public octet::app {
		// Matrix to transform points in our camera space to the world.
		// This lets us move our camera
		mat4t cameraToWorld;

		// shader to draw a textured triangle
		texture_shader texture_shader_;

		enum {
			num_music_sound_source = 1,
			num_sound_sources = 8,
			num_missiles = 4,
			num_bombs = 2,
			num_explosions = 8,
			num_borders = 4,

			//6big +12small +24tiny
			num_big_asteroids = 6,
			num_small_asteroids = num_big_asteroids * 2,
			num_tiny_asteroids = num_small_asteroids * 2,
			num_asteroids = num_big_asteroids + num_small_asteroids + num_tiny_asteroids,


			// sprite definitions
			ship_sprite = 0,
			start_sprite,
			press_start_sprite,
			game_over_sprite,

			shipC_sprite,
			shipSpriteFull,
			shipSpriteEmpty,
			explosionSprite,

			first_explosion_sprite,
			last_explosion_sprite = first_explosion_sprite + num_explosions - 1,

			first_asteroid_big_sprite,
			last_asteroid_big_sprite = first_asteroid_big_sprite + num_big_asteroids - 1,

			first_asteroid_small_sprite,
			last_asteroid_small_sprite = first_asteroid_small_sprite + num_small_asteroids - 1,

			first_asteroid_tiny_sprite,
			last_asteroid_tiny_sprite = first_asteroid_tiny_sprite + num_tiny_asteroids - 1,

			first_missile_sprite,
			last_missile_sprite = first_missile_sprite + num_missiles - 1,

			first_bomb_sprite,
			last_bomb_sprite = first_bomb_sprite + num_bombs - 1,

			first_border_sprite,
			last_border_sprite = first_border_sprite + num_borders - 1,

			num_sprites,

		};

		float ship_speed = 0;

		float blastfloat = 0;		//if blastfloat is >0.5f, be near the player, else warp away

		// timers for missiles and bombs
		int missiles_disabled;
		int bombs_disabled;

		int bigCounter = 0;
		int smallCounter = 0;
		int tinyCounter = 0;

		//if game is not playing
		bool titleScreen = true;

		int resetTimer = 60;

		// accounting for bad guys
		int live_asteroids;

		//player lives!!!
		int num_lives;

		bool dead = false;
		int deathTimer = 0;
		// game state
		bool game_over;
		int score;

		//for camera shakey
		float camerashake = 0;

		// speed of enemy
		float asteroid_velocity;

		// sounds
		ALuint whoosh;
		ALuint bang;

		ALuint powerup;

		ALuint enemy_hit;
		ALuint enemy_shoot;

		ALuint player_shoot;
		ALuint player_hurt;
		ALuint player_dead;

		//
		ALuint music;			//need to find out how to loop
		//
		unsigned cur_source;
		ALuint sources[num_sound_sources];
		unsigned cur_music_source;
		ALuint sources2[num_music_sound_source];
		//

		// big array of sprites
		sprite sprites[num_sprites];

		// random number generator
		class random randomizer;

		// a texture for our text
		GLuint font_texture;

		// information for our text
		bitmap_font font;

		ALuint get_sound_source() { return sources[cur_source++ % num_sound_sources]; }
		ALuint get_music_source() { return sources2[0]; }

		//create Invaders
		void create_Asteroids()
		{

			GLuint asteroid = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/asteroid1.gif");
			GLuint asteroid2 = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/asteroid1.gif");
			GLuint asteroid3 = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/asteroid1.gif");

			//first 6 are big
			for (int k = 0; k != num_big_asteroids; ++k)
			{
				assert(first_asteroid_big_sprite + k <= last_asteroid_big_sprite);

				float pointNum = (k*0.5f) / num_big_asteroids;
				float angle = pointNum*3.14159265;
				float x = sin(angle*4) * 2.5f;
				float y = cos(angle*4) * 2.5f;

				sprites[first_asteroid_big_sprite + k].init(
					asteroid, x,y, 0.6f, 0.6f);
				sprites[first_asteroid_big_sprite + k].xDir = randomizer.get(-10, 10) * (0.003f);
				sprites[first_asteroid_big_sprite + k].yDir = randomizer.get(-10, 10) * (0.003f);
				float initAsteroidRot = randomizer.get(0, 360);
				sprites[first_asteroid_big_sprite + k].rotate(initAsteroidRot);
				sprites[first_asteroid_big_sprite + k].missileRotation = initAsteroidRot;
				sprites[first_asteroid_big_sprite + k].asteroidSize = 1;
			}
			//next few are smaller and start dead
			for (int l = 0; l != num_small_asteroids; ++l)
			{
				assert(first_asteroid_small_sprite + l <= last_asteroid_small_sprite);

				sprites[first_asteroid_small_sprite + l].init(
					asteroid2, 20, 0, 0.4f, 0.4f);
				sprites[first_asteroid_small_sprite + l].is_enabled() = false;
				sprites[first_asteroid_small_sprite + l].xDir = randomizer.get(-10,10) * (0.006f);
				sprites[first_asteroid_small_sprite + l].yDir = randomizer.get(-10, 10) * (0.006f);
				float initAsteroidRot = randomizer.get(0, 360);
				sprites[first_asteroid_small_sprite + l].rotate(initAsteroidRot);
				sprites[first_asteroid_small_sprite + l].missileRotation = initAsteroidRot;
				sprites[first_asteroid_small_sprite + l].asteroidSize = 2;
			}

			//and the rest.
			for (int m = 0; m != num_tiny_asteroids; ++m)
			{
				assert(first_asteroid_tiny_sprite + m <= last_asteroid_tiny_sprite);

				sprites[first_asteroid_tiny_sprite + m].init(
					asteroid3, 20, 0, 0.3f, 0.3f);
				sprites[first_asteroid_tiny_sprite + m].is_enabled() = false;
				sprites[first_asteroid_tiny_sprite + m].xDir = randomizer.get(-10, 10) * (0.008f);
				sprites[first_asteroid_tiny_sprite + m].yDir = randomizer.get(-10, 10) * (0.008f);
				float initAsteroidRot = randomizer.get(0, 360);
				sprites[first_asteroid_tiny_sprite + m].rotate(initAsteroidRot);
				sprites[first_asteroid_tiny_sprite + m].missileRotation = initAsteroidRot;
				sprites[first_asteroid_tiny_sprite + m].asteroidSize = 3;
			}
		}

		//guess
		void create_Player()
		{
			GLuint ship = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/ship.gif");
			sprites[ship_sprite].init(ship, 0, 0, 0.22f, 0.22f);

			GLuint shipblastfull = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/shipBlastFull.gif");
			sprites[shipSpriteFull].init(shipblastfull, 0, 2, 0.25f, 0.25f);

			GLuint explosion = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/explosion.gif");
			for (int i = 0; i != num_explosions; ++i)
			{
				sprites[explosionSprite + i].init(explosion, 20, 0, 0.15f, 0.15f);
				sprites[explosionSprite + i].is_enabled() = false;
			}

		}
		// called when a bullet hits an enemy
		void on_hit_asteroid() {

			ALuint source = get_sound_source();
			alSourcei(source, AL_BUFFER, enemy_hit);
			alSourcePlay(source);

			live_asteroids--;
			score++;
			printf("\n live asteroids: %i", live_asteroids);
		}

		void TitleScreen()
		{
			if (is_key_down(key_space) && titleScreen)
			{
				sprites[start_sprite].translate(40, 0);

				create_Asteroids();
				create_Player();

				titleScreen = false;
			}

		}
		// called when we are hit
		void on_hit_ship() {
			ALuint source = get_sound_source();
			alSourcei(source, AL_BUFFER, player_hurt);
			alSourcePlay(source);

			//sprites[ship_sprite].set_relative(sprites[first_border_sprite + 3],-3.2f,0);

			if (--num_lives == 0) {
				game_over = true;
				sprites[game_over_sprite].translate(-20, 0);

			}
		}

		// use the keyboard to move the ship
		void move_ship() {

			const float shipRotateSpeed = 8;

			if (is_key_down(key_left)) {
				sprites[ship_sprite].rotate(shipRotateSpeed);
			}

			else if (is_key_down(key_right)) {
				sprites[ship_sprite].rotate(-shipRotateSpeed);
			}

			if (is_key_going_down(key_space))
			{
				blastfloat = 1;			//for blast graphic.
			}

			if (is_key_down(key_space))
			{
				//sprites[ship_sprite].velocity = vec3(sprites[ship_sprite].modelToWorld[3][0], sprites[ship_sprite].modelToWorld[3][1], 0);

				sprites[ship_sprite].translate(0, ship_speed / 240);

				//inhibit top speed
				if (ship_speed < 16)
					ship_speed += 0.8f;
				else
					ship_speed = 16;

				//for the blast graphic.
				if (blastfloat > 0)
					blastfloat -= 0.5f;
				else
					blastfloat = 1;

				//just warps the blast graphic from offscreen to player
				if (blastfloat > 0.5f)
					sprites[shipSpriteFull].set_relative(sprites[ship_sprite], 0, -0.04f);
				else
					sprites[shipSpriteFull].translate(20, 20);
			}

			//printf("%f %f \n", sprites[ship_sprite].modelToWorld[3][0], sprites[ship_sprite].modelToWorld[3][1]);
			//printf("%f \n", sprites[ship_sprite].velocity);

			if (!is_key_down(key_space))
			{
				if (ship_speed > 0)
					ship_speed -= 0.2f;		//decelerates a lot slower!!
				else
					ship_speed = 0;

				sprites[ship_sprite].translate(0, ship_speed / 240);

				//warp blastgraphic away.
				sprites[shipSpriteFull].translate(20, 20);
			}


			for (int j = 0; j != num_asteroids; ++j)
			{
				sprite &asteroid = sprites[first_asteroid_big_sprite + j];

				if (asteroid.is_enabled() && sprites[ship_sprite].collides_with(asteroid))
				{
					deathTimer = 40;
					num_lives--;
					camerashake = 4;
					sprites[ship_sprite].is_enabled() = false;

					for (int i = 0; i != num_explosions; ++i)
					{
						sprite &explosion = sprites[first_explosion_sprite + i];
						//makes 7 explosions...
						int rotateAngle = (int)51.4285f;
						explosion.explosion_speed = 0.18f;
						explosion.set_relative(sprites[ship_sprite], 0, 0);
						explosion.rotate((float)((i)* rotateAngle));
						explosion.is_enabled() = true;
					}
				}
			}


			if (deathTimer > 0)
			{
				deathTimer--;
				dead = true;
				sprites[ship_sprite].set_relative(sprites[first_border_sprite + 3], 20, 0);
			}
			else if (deathTimer == 0 && dead)
			{
				//move the 'press space to start' to screen
				sprites[press_start_sprite].set_relative(sprites[first_border_sprite + 3], -3.2f, 0);

				if (is_key_going_down(key_space))
				{
					//reset player position, essentially
					deathTimer = 0;
					sprites[press_start_sprite].set_relative(sprites[first_border_sprite + 3], 30, 0);
					sprites[ship_sprite].set_relative(sprites[first_border_sprite + 3], -3.2f, 0);
					sprites[ship_sprite].playerRotation = 0;
					sprites[ship_sprite].is_enabled() = true;
					deathTimer = 0;
					dead = false;
				}
			}

			//if you touch the right wall...
			if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 3]))
			{
				float tempRotation = sprites[ship_sprite].playerRotation;
				sprites[ship_sprite].rotate(-sprites[ship_sprite].playerRotation);		//make rotation 0
				sprites[ship_sprite].set_relative(sprites[ship_sprite], -6, 0);			//move it left
				sprites[ship_sprite].rotate(tempRotation);								//return rotation to orginial rot
			}
			//left wall...
			if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 2]))
			{
				float tempRotation = sprites[ship_sprite].playerRotation;
				sprites[ship_sprite].rotate(-sprites[ship_sprite].playerRotation);		//make rotation 0
				sprites[ship_sprite].set_relative(sprites[ship_sprite], 6, 0);			//move it left
				sprites[ship_sprite].rotate(tempRotation);								//return rotation to orginial rot
			}
			//bottom wall...
			if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 0]))
			{
				float tempRotation = sprites[ship_sprite].playerRotation;
				sprites[ship_sprite].rotate(-sprites[ship_sprite].playerRotation);		//make rotation 0
				sprites[ship_sprite].set_relative(sprites[ship_sprite], 0, 6);			//move it left
				sprites[ship_sprite].rotate(tempRotation);								//return rotation to orginial rot
			}
			//top wall...
			if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 1]))
			{
				float tempRotation = sprites[ship_sprite].playerRotation;
				sprites[ship_sprite].rotate(-sprites[ship_sprite].playerRotation);		//make rotation 0
				sprites[ship_sprite].set_relative(sprites[ship_sprite], 0, -6);			//move it left
				sprites[ship_sprite].rotate(tempRotation);								//return rotation to orginial rot
			}

		}

		void move_explosions()
		{

			for (int i = 0; i != num_explosions; ++i)
			{
				sprite &explosion = sprites[first_explosion_sprite + i];


				if (explosion.explosion_speed > 0)
				{
					explosion.explosion_speed -= 0.01f;
				}
				else
				{
					explosion.explosion_speed = (0, 0, 0);
					explosion.rotate(4);
					if (explosion.modelScale == 0.1f)
					{
						explosion.modelScale = 1;
						explosion.scale(10);
						explosion.set_relative(sprites[first_border_sprite + 3], 30, 0);
						explosion.is_enabled() = false;
					}

				}

				if (explosion.is_enabled())
				{
					explosion.translate(0, -explosion.explosion_speed);
					explosion.scale(0.9f);
				}

			}
		}

		// fire button (space)
		void fire_missiles()
		{
			if (missiles_disabled)
			{
				--missiles_disabled;
			}
			else if (is_key_going_down(key_ctrl) && !titleScreen && sprites[ship_sprite].is_enabled())
			{
				for (int i = 0; i != num_missiles; ++i)
				{
					if (!sprites[first_missile_sprite + i].is_enabled())
					{
						sprites[first_missile_sprite + i].is_enabled() = true;
						//life of missile
						sprites[first_missile_sprite + i].missileTimer = 20;

						//warp and set rotation
						sprites[first_missile_sprite + i].set_relative(sprites[ship_sprite], 0, 0.02f);
						sprites[first_missile_sprite + i].missileRotation = sprites[ship_sprite].playerRotation;

						//TIMER INBETWEEN PLAYER SHOTS!!!
						missiles_disabled = 1;
						ALuint source = get_sound_source();
						alSourcei(source, AL_BUFFER, player_shoot);
						alSourcePlay(source);

						if (camerashake <= 2)
							camerashake += 0.5f;

						break;
					}
				}
			}
		}

		// animate the missiles
		void move_missiles() {

			for (int i = 0; i != num_missiles; ++i)
			{
				sprite &missile = sprites[first_missile_sprite + i];

				if (missile.is_enabled())
				{
					float missile_speed = 0.26f;

					missile.missileTimer--;

					missile.translate(0, missile_speed);

					for (int j = 0; j < num_big_asteroids; ++j)
					{
						sprite &asteroid = sprites[first_asteroid_big_sprite + j];
						// if bullet hits asteroid
						if (missile.collides_with(asteroid))
						{
							bigCounter++;
							missile.is_enabled() = false;
							missile.translate(20, 0);

							for (int a = 0; a < num_small_asteroids; a++)
							{
								if (!sprites[first_asteroid_small_sprite + a].is_enabled()
									&& !sprites[first_asteroid_small_sprite + a+1].is_enabled())
								{
									//warp 2 smaller ones over to you...
									for (int x = 0; x < 2; x++)
									{
										sprites[first_asteroid_small_sprite + a + x].set_relative(asteroid, 0, 0);
										sprites[first_asteroid_small_sprite + a + x].rotate(-asteroid.missileRotation);
										sprites[first_asteroid_small_sprite + a + x].missileRotation = 0;
										sprites[first_asteroid_small_sprite + a + x].is_enabled() = true;
										if (x == 1)
										{
											printf("\n ting hit.");
											asteroid.is_enabled() = false;
											on_hit_asteroid();
											asteroid.translate(20, 0);
											goto next_missile;
										}
									}
								}
							}
						}
					}
					for (int j = 0; j < num_small_asteroids; ++j)
					{
						sprite &asteroid = sprites[first_asteroid_small_sprite + j];
						// if bullet hits asteroid
						if (missile.collides_with(asteroid))
						{
							smallCounter++;
							missile.is_enabled() = false;
							missile.translate(20, 0);

							for (int a = 0; a < num_tiny_asteroids; a++)
							{
								if (!sprites[first_asteroid_tiny_sprite + a].is_enabled()
									&& !sprites[first_asteroid_tiny_sprite + a + 1].is_enabled())
								{
									for (int x = 0; x < 2; x++)
									{
										sprites[first_asteroid_tiny_sprite + a + x].set_relative(asteroid, 0, 0);
										sprites[first_asteroid_tiny_sprite + a + x].rotate(-asteroid.missileRotation);
										sprites[first_asteroid_tiny_sprite + a + x].missileRotation = 0;
										sprites[first_asteroid_tiny_sprite + a + x].is_enabled() = true;
										if (x == 1)
										{
											printf("\n ting hit.");
											asteroid.is_enabled() = false;
											on_hit_asteroid();
											asteroid.translate(20, 0);
											goto next_missile;
										}
									}
								}
							}
						}
					}
					for (int j = 0; j != num_tiny_asteroids; ++j)
					{
						sprite &asteroid = sprites[first_asteroid_tiny_sprite + j];

						if (missile.collides_with(asteroid))
						{
							tinyCounter++;
							missile.is_enabled() = false;
							missile.translate(20, 0);
							asteroid.is_enabled() = false;
							on_hit_asteroid();
							asteroid.translate(20, 0);
							printf("hit a tiny fucker");
							goto next_missile;
						}
					}

					//when time runs out, disable missile.
					if (missile.missileTimer <= 0)
					{
						missile.is_enabled() = false;
						missile.translate(20, 0);
						goto next_missile;
					}
				}

				for (int i = 0; i < 4; i++)
				{
					if (missile.collides_with(sprites[first_border_sprite + i]))
					{
						float tempRotation = missile.missileRotation;
						missile.rotate(-missile.missileRotation);		//make rotation 0
						//
						if (missile.collides_with(sprites[first_border_sprite + 3]))
						{
							missile.set_relative(missile, -6, 0);			//move it left
						}
						//left wall...
						if (missile.collides_with(sprites[first_border_sprite + 2]))
						{
							missile.set_relative(missile, 6, 0);			//move it left
						}
						//bottom wall...
						if (missile.collides_with(sprites[first_border_sprite + 0]))
						{
							missile.set_relative(missile, 0, 6);			//move it left
						}
						//top wall...
						if (missile.collides_with(sprites[first_border_sprite + 1]))
						{
							missile.set_relative(missile, 0, -6);		//move it left
						}
						//
						missile.rotate(tempRotation);					//return rotation to orginial rot
					}
				}
			}
		next_missile:;

			if (is_key_going_down(key_alt))
			{
				printf("\n \n %i, %i, %i.", bigCounter, smallCounter, tinyCounter);
			}
		}

		// animate the bombs
		void move_bombs() {
			const float bomb_speed = 0.2f;
			for (int i = 0; i != num_bombs; ++i) {
				sprite &bomb = sprites[first_bomb_sprite + i];
				if (bomb.is_enabled()) {
					bomb.translate(0, -bomb_speed);
					if (bomb.collides_with(sprites[ship_sprite])) {
						bomb.is_enabled() = false;
						bomb.translate(20, 0);
						bombs_disabled = 50;
						on_hit_ship();
						goto next_bomb;
					}
					if (bomb.collides_with(sprites[first_border_sprite + 0])) {
						bomb.is_enabled() = false;
						bomb.translate(20, 0);
					}
				}
			next_bomb:;
			}
		}

		// move the array of asteroids
		void move_asteroids()
		{
			for (int j = 0; j != num_asteroids; ++j)
			{
				sprite &asteroid = sprites[first_asteroid_big_sprite + j];

				if (asteroid.is_enabled())
				{
					asteroid.translate(asteroid.xDir * asteroid_velocity, asteroid.yDir * asteroid_velocity);

					for (int i = 0; i < 4; i++)
					{
						if (asteroid.collides_with(sprites[first_border_sprite + i]))
						{
							float tempRotation = asteroid.missileRotation;		//store rotation
							asteroid.rotate(-asteroid.missileRotation);			//make rotation 0
							//
							//if it hits right wall...
							if (asteroid.collides_with(sprites[first_border_sprite + 3]))
							{
								asteroid.set_relative(asteroid, -6, 0);			//move it left
							}
							//left wall...
							if (asteroid.collides_with(sprites[first_border_sprite + 2]))
							{
								asteroid.set_relative(asteroid, 6, 0);			//move it right
							}
							//bottom wall...
							if (asteroid.collides_with(sprites[first_border_sprite + 0]))
							{
								asteroid.set_relative(asteroid, 0, 6);			//move it up
							}
							//top wall...
							if (asteroid.collides_with(sprites[first_border_sprite + 1]))
							{
								asteroid.set_relative(asteroid, 0, -6);			//move it down
							}
							//
							asteroid.rotate(tempRotation);						//return rotation to original rot
						}
					}
				}
			}


		}

		void resetCheck()
		{
			if (live_asteroids == 0)
			{
				resetTimer--;
				printf("\n move all asteroids now!!!");
				if (resetTimer <= 0)
				{
					for (int i = 0; i < num_big_asteroids; i++)
					{
						sprite &asteroid = sprites[first_asteroid_big_sprite + i];
						float pointNum = (i*0.5f) / num_big_asteroids;
						float angle = pointNum*3.14159265;
						float x = sin(angle * 4) * 2.5f;
						float y = cos(angle * 4) * 2.5f;
						asteroid.modelToWorld[3][0] = x;
						asteroid.modelToWorld[3][1] = y;
						asteroid.is_enabled() = true;
					}
					live_asteroids = num_asteroids;
					asteroid_velocity += 0.15f;
					resetTimer = 60;
				}
			}
		}
		void draw_text(texture_shader &shader, float x, float y, float scale, const char *text) {
			mat4t modelToWorld;
			modelToWorld.loadIdentity();
			modelToWorld.translate(x, y, 0);
			modelToWorld.scale(scale, scale, 1);
			mat4t modelToProjection = mat4t::build_projection_matrix(modelToWorld, cameraToWorld);

			/*mat4t tmp;
			glLoadIdentity();
			glTranslatef(x, y, 0);
			glGetFloatv(GL_MODELVIEW_MATRIX, (float*)&tmp);
			glScalef(scale, scale, 1);
			glGetFloatv(GL_MODELVIEW_MATRIX, (float*)&tmp);*/

			enum { max_quads = 32 };
			bitmap_font::vertex vertices[max_quads * 4];
			uint32_t indices[max_quads * 6];
			aabb bb(vec3(0, 0, 0), vec3(256, 256, 0));

			unsigned num_quads = font.build_mesh(bb, vertices, indices, max_quads, text, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, font_texture);

			shader.render(modelToProjection, 0);

			glVertexAttribPointer(attribute_pos, 3, GL_FLOAT, GL_FALSE, sizeof(bitmap_font::vertex), (void*)&vertices[0].x);
			glEnableVertexAttribArray(attribute_pos);
			glVertexAttribPointer(attribute_uv, 3, GL_FLOAT, GL_FALSE, sizeof(bitmap_font::vertex), (void*)&vertices[0].u);
			glEnableVertexAttribArray(attribute_uv);

			glDrawElements(GL_TRIANGLES, num_quads * 6, GL_UNSIGNED_INT, indices);
		}

	public:

		// this is called when we construct the class
		invaderers_app(int argc, char **argv) : app(argc, argv), font(512, 256, "assets/big.fnt") {
		}

		// this is called once OpenGL is initialized
		void app_init() {

			// set up the shader
			texture_shader_.init();

			// set up the matrices with a camera 5 units from the origin
			cameraToWorld.loadIdentity();
			cameraToWorld.translate(0, 0, 3);

			font_texture = resource_dict::get_texture_handle(GL_RGBA, "assets/big_0.gif");

			GLuint GameOver = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/GameOver.gif");
			sprites[game_over_sprite].init(GameOver, 20, 0, 3, 1.5f);

			GLuint Start = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/Start.gif");
			sprites[start_sprite].init(Start, 0, 0, 5, 4);

			GLuint PressSpace = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/PressSpaceToContinue.gif");
			sprites[press_start_sprite].init(PressSpace, 30, 0, 5, 4);

			GLuint asteroid = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/invaderer2.gif");

			// set the border to white for clarity
			GLuint white = resource_dict::get_texture_handle(GL_RGB, "#ffffff");
			GLuint black = resource_dict::get_texture_handle(GL_RGB, "#000000");
			sprites[first_border_sprite + 0].init(black, 0, -3.3f, 10, 0.2f);		//bottom
			sprites[first_border_sprite + 1].init(black, 0, 3.3f, 10, 0.2f);		//top
			sprites[first_border_sprite + 2].init(black, -3.3f, 0, 0.2f, 10);		//left
			sprites[first_border_sprite + 3].init(black, 3.3f, 0, 0.2f, 10);		//guess

			// use the missile texture
			GLuint missile = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/missile.gif");
			for (int i = 0; i != num_missiles; ++i) {
				// create missiles off-screen
				sprites[first_missile_sprite + i].init(missile, 0, 0, 0.08f, 0.08f);
				sprites[first_missile_sprite + i].is_enabled() = false;
			}



			// sounds
			whoosh = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/whoosh.wav");
			bang = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/bang.wav");
			music = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/music.wav");

			powerup = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/powerup.wav");


			player_shoot = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/player_shoot.wav");
			player_hurt = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/player_hurt.wav");
			player_dead = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/player_dead.wav");

			enemy_shoot = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/enemy_shoot.wav");
			enemy_hit = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/enemy_hit.wav");



			cur_source = 0;
			alGenSources(num_sound_sources, sources);
			alGenSources(num_music_sound_source, sources2);

			// sundry counters and game state.
			missiles_disabled = 0;
			bombs_disabled = 50;		//5 seconds before enemies can shoot
			asteroid_velocity = 1.0f;
			live_asteroids = num_asteroids;
			num_lives = 3;
			game_over = false;
			score = 0;


			ALuint source2 = get_music_source();
			//ASK HOW TO MAKE IT LOOP!!! OK, i just gotta do a dutty hack for it...
			alSourcei(source2, AL_BUFFER, music);
			alSourcePlay(source2);

		}

		// called every frame to move things
		void simulate() {
			if (game_over) {
				return;
			}

			TitleScreen();

			move_ship();

			fire_missiles();

			//fire_bombs();

			move_missiles();

			move_bombs();

			move_explosions();

			move_asteroids();

			resetCheck();

			//FileReader().ExtractFileContents();

			//Camera Shake
			if (camerashake > 0)
			{
				camerashake -= 0.1f;
			}
			else
			{
				camerashake = 0;
				cameraToWorld[3][0] = 0;
				cameraToWorld[3][1] = 0;
			}
			cameraToWorld[3][0] = camerashake*(randomizer.get(-.5f,1.5f)*0.02f);
			cameraToWorld[3][1] = camerashake*(randomizer.get(-.5f, 1.5f)*0.02f);
			cameraToWorld[3][2] = 3 - camerashake*(randomizer.get(-.5f, 1.5f)*0.08f);
		}

		// this is called to draw the world
		void draw_world(int x, int y, int w, int h) {
			simulate();

			// set a viewport - includes whole window area
			glViewport(x, y, w, h);

			// clear the background to black
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// don't allow Z buffer depth testing (closer objects are always drawn in front of far ones)
			glDisable(GL_DEPTH_TEST);

			// allow alpha blend (transparency when alpha channel is 0)
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// draw all the sprites
			for (int i = 0; i != num_sprites; ++i) {
				sprites[i].render(texture_shader_, cameraToWorld);
			}

			char score_text[32];
			sprintf(score_text, "score: %d			 lives: %d\n", score, num_lives);
			draw_text(texture_shader_, -1.75f, 2, 1.0f / 256, score_text);

			// move the listener with the camera
			vec4 &cpos = cameraToWorld.w();
			alListener3f(AL_POSITION, cpos.x(), cpos.y(), cpos.z());
		}
	};

	class FileReader {

	public:

		void ExtractFileContents()
		{
			std::ifstream my_file("location/ofMy/file.csv");

			if (my_file.bad()) {  // Checks if something isn't right before we blunder ahead assuming it is!
				printf("Can't read the file 😞");
			}
			else
			{
				while (my_file.eof() != true)
				{
					char buffer[100];  // Temporarily store each line here (recreated for each line)
									   //std::getline(my_file, buffer, ',')
									   //  Now loop across the buffer char array and do summut with each char.

									   // Loop repeats for each new line.
				}

			}

		}

	};

}

