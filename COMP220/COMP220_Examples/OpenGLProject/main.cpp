//main.cpp - define the entry point of the application

#include "main.h"

void LoadImageSupport() {
	int flags = IMG_INIT_JPG | IMG_INIT_PNG;
	int initted = IMG_Init(flags);
	if ((initted&flags) != flags)
	{
		printf("IMG_Init: Failed to init required jpg and png support.\n");
		printf("IMG_Init: %s\n", IMG_GetError());
	}
}

void RequestCoreOpenGL(int majorVersion, int minorVersion) {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minorVersion);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

int InitialiseSDLWindowAndOpenGL(int windowWidth, int windowHeight) {
	// load support for JPG and PNG image formats (tiffs also supported, but not checked)
	LoadImageSupport();
	//Request 3.1 core OpenGL
	RequestCoreOpenGL(3, 1);

	//Initialises the SDL Library, passing in SDL_INIT_VIDEO to only initialise the video subsystems
	//https://wiki.libsdl.org/SDL_Init
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		return 1;
	}

	//Create a window, note we have to free the pointer returned using the DestroyWindow Function
	//https://wiki.libsdl.org/SDL_CreateWindow
	window = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	//Checks to see if the window has been created, the pointer will have a value of some kind
	if (window == nullptr)
	{
		return 1;
	}

	// Hide mouse cursor
	SDL_ShowCursor(SDL_DISABLE);

	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	if (glContext == nullptr)
	{
		return 1;
	}

	//Init GLEW
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		return 1;
	}

	return 0;
}

void QuitSDL() {
	// Clean up
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
};

int main(int argc, char* args[])
{
	// Define window size
	int windowWidth = 800;
	int windowHeight = 800;

	// Check SDL initialisation
	if (InitialiseSDLWindowAndOpenGL(windowWidth, windowHeight) == 1) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, SDL_GetError(), "SDL and openGL failed to initialise", NULL);
		QuitSDL();
	}

	// Add tanks
	GameObject tank(vec3(10.0f, 0.0f, 0.0f), vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f));
	tank.Update();

	// Second tank
	GameObject secondTank(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f));
	secondTank.Update();

	// Add camera
	Camera playerCamera;

	// Lighting
	Lighting light;

	// Material
	Material material;

	// Color buffer texture
	GLuint colorBufferID = CreateTexture(windowWidth, windowHeight);

	// Create depth buffer
	GLuint depthRenderBufferID;
	glGenRenderbuffers(1, &depthRenderBufferID);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBufferID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, windowWidth, windowHeight);

	// Create frame buffer
	GLuint frameBufferID;
	glGenFramebuffers(1, &frameBufferID);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBufferID);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorBufferID, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unable to create frame buffer for post processing", "Frame buffer error", NULL);
	}

	// Create sceen aligned quad
	GLfloat screenVerts[] =
	{
		-1, -1,
		1, -1,
		-1, 1,
		1, 1
	};

	GLuint screenQuadVBOID;
	glGenBuffers(1, &screenQuadVBOID);
	glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBOID);
	glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), screenVerts, GL_STATIC_DRAW);

	GLuint screenVAO;
	glGenVertexArrays(1, &screenVAO);
	glBindVertexArray(screenVAO);
	glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBOID);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	GLuint postProcessingProgramID = LoadShaders("passThroughVert.glsl", "postTextureFrag.glsl");
	GLint texture0Location = glGetUniformLocation(postProcessingProgramID, "texture0");
	if (texture0Location < 0)
	{
		printf("Unable to find %s uniform", "texture0");
	}

	//Create and compile GLSL program from shaders

	tank.CreateUniformLocations();
	secondTank.CreateUniformLocations();

	static const GLfloat fragColor[] = { 0.0f, 1.0f, 0.0f, 1.0f };

	// Lighting
	light.InitialiseUniformLocations(tank.programID);
	light.InitialiseUniformLocations(secondTank.programID);

	// Material
	material.InitialiseUniformLocations(tank.programID);
	material.InitialiseUniformLocations(secondTank.programID);

	///-----bullet initialization_start-----

	///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();

	///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);

	///btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
	btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();

	///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

	btDiscreteDynamicsWorld* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);

	dynamicsWorld->setGravity(btVector3(0, -10, 0));

	///-----bullet initialization_end-----

	// Creating ground with bullet
	btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(2.), btScalar(50.)));

	btTransform groundTransform;
	groundTransform.setIdentity();
	groundTransform.setOrigin(btVector3(0, -10, 0));

	btScalar mass(0.);
	btVector3 localInertia(0, 0, 0);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, groundShape, localInertia);
	btRigidBody* groundRigidBody = new btRigidBody(rbInfo);

	//add the body to the dynamics world
	dynamicsWorld->addRigidBody(groundRigidBody);

	// Create tank collision
	tank.CreateRigidBody();
	dynamicsWorld->addRigidBody(tank.objectRigidBody);

	// Second tank collision
	secondTank.CreateRigidBody();
	dynamicsWorld->addRigidBody(secondTank.objectRigidBody);

	glEnable(GL_DEPTH_TEST);
	int lastTicks = SDL_GetTicks();
	int currentTicks = SDL_GetTicks();



	SDL_WarpMouseInWindow(window, windowWidth / 2, windowHeight / 2);

	InputManager inputs;

	//Event loop, we will loop until running is set to false, usually if escape has been pressed or window is closed
	bool running = true;

	//SDL Event structure, this will be checked in the while loop
	SDL_Event event;

	while (running)
	{
		// Get keypresses
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_KEYDOWN)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_UP:
					// Start increasing mouse sensitivity
					inputs.mouseSensitivity.SetPositive();
					break;

				case SDLK_DOWN:
					// Start decreasing mouse sensitivity
					inputs.mouseSensitivity.SetNegative();
					break;

				case SDLK_ESCAPE:
					running = false;
					break;

				case SDLK_w:
					inputs.forward.SetPositive();
					break;

				case SDLK_a:
					inputs.sideways.SetPositive();
					break;

				case SDLK_s:
					inputs.forward.SetNegative();
					break;

				case SDLK_d:
					inputs.sideways.SetNegative();
					break;

				case SDLK_SPACE:
					inputs.jump.SetPositive();
					inputs.fly.SetPositive();
					break;

				case SDLK_LSHIFT:
					inputs.fly.SetNegative();
					break;

				case SDLK_f:
					// enable fly
					playerCamera.flying = true;

				default:
					break;
				}
			}
			if (event.type == SDL_KEYUP)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_UP:
					// Stop mouse sensitivity increasing
					inputs.mouseSensitivity.ZeroPositive();
					break;

				case SDLK_DOWN:
					// Stop mouse sensitivity decreasing
					inputs.mouseSensitivity.ZeroNegative();
					break;

				case SDLK_w:
					inputs.forward.ZeroPositive();
					break;

				case SDLK_a:
					inputs.sideways.ZeroPositive();
					break;

				case SDLK_s:
					inputs.forward.ZeroNegative();
					break;

				case SDLK_d:
					inputs.sideways.ZeroNegative();
					break;

				case SDLK_SPACE:
					inputs.jump.ZeroPositive();
					inputs.fly.ZeroPositive();
					break;

				case SDLK_LSHIFT:
					inputs.fly.ZeroNegative();
					break;

				default:
					break;
				}
			}
		}

		// Run through inputs
		// Forward and backward movement
		if (inputs.forward.GetValue() != 0)
		{
			playerCamera.Forward(inputs.forward.GetValue());
		}

		// Sideways movement
		if (inputs.sideways.GetValue() != 0)
		{
			playerCamera.Sideways(inputs.sideways.GetValue());
		}

		// Jumping
		if (inputs.jump.GetValue() != 0 || playerCamera.jumping == true) {
			playerCamera.Jump();  
		}
		
		// Listen for playerCamera fly events
		playerCamera.Fly(inputs.fly.GetValue());

		// Listen for mouse sensitivity adjustments
		if (inputs.mouseSensitivity.GetValue() != 0)
		{
			playerCamera.AdjustMouseSensitivity(inputs.mouseSensitivity.GetValue());
		}

		currentTicks = SDL_GetTicks();
		float deltaTime = (float)(currentTicks - lastTicks) / 1000.0f;

		dynamicsWorld->stepSimulation(1.f / 60.f, 10);
		

		// Snap mouse to center
		SDL_WarpMouseInWindow(window, windowWidth / 2, windowHeight / 2);

		// Move camera based on mouse movement
		playerCamera.MoveView(windowWidth, windowHeight);
		playerCamera.ApplyGravity(groundTransform.getOrigin().getY() + 3.0f);

		//Recalculate object position
		tank.Update();
		secondTank.Update();

		glEnable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);

		// Clear the screen
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClearDepth(1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tank.textureID);

		glUseProgram(tank.programID);
		glUniform4fv(tank.fragColorLocation, 1, fragColor);
		tank.UseUniformLocations(currentTicks, playerCamera);

		// Lighting
		light.UseUniformLocations(tank.programID);

		// Material
		material.UseUniformLocations(tank.programID, light);
		
		//Draw the triangle
		tank.Draw();

		// Second tank
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, secondTank.textureID);

		glUseProgram(secondTank.programID);
		glUniform4fv(secondTank.fragColorLocation, 1, fragColor);
		tank.UseUniformLocations(currentTicks, playerCamera);

		// Lighting
		light.UseUniformLocations(secondTank.programID);

		// Material
		material.UseUniformLocations(secondTank.programID, light);

		//Draw the triangle
		secondTank.Draw();

		glDisable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Bind post processing shaders
		glUseProgram(postProcessingProgramID);
		
		// Activate texture unit 0 for the color buffer
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorBufferID);
		glUniform1i(texture0Location, 0);

		glBindVertexArray(screenVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		SDL_GL_SwapWindow(window);

		lastTicks = currentTicks;
	}
	
	// Destroy models
	dynamicsWorld->removeRigidBody(tank.objectRigidBody);
	tank.Destroy();

	dynamicsWorld->removeRigidBody(secondTank.objectRigidBody);
	secondTank.Destroy();

	dynamicsWorld->removeRigidBody(groundRigidBody);
	// Delete ground
	delete groundShape;
	delete groundRigidBody->getMotionState();
	delete groundRigidBody;

	// Delete bullet content
	//delete dynamics world
	delete dynamicsWorld;

	//delete solver
	delete solver;

	//delete broadphase
	delete overlappingPairCache;

	//delete dispatcher
	delete dispatcher;

	delete collisionConfiguration;

	// Delete post processing from memory
	glDeleteProgram(postProcessingProgramID);
	glDeleteVertexArrays(1, &screenVAO);
	glDeleteBuffers(1, &screenQuadVBOID);
	glDeleteFramebuffers(1, &frameBufferID);
	glDeleteRenderbuffers(1, &depthRenderBufferID);
	glDeleteTextures(1, &colorBufferID);

	SDL_GL_DeleteContext(glContext);
	//Destroy the window and quit SDL2, NB we should do this after all cleanup in this order!!!
	//https://wiki.libsdl.org/SDL_DestroyWindow
	SDL_DestroyWindow(window);

	IMG_Quit();

	//https://wiki.libsdl.org/SDL_Quit
	SDL_Quit();

	return 0;
}
