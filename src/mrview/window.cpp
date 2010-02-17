/*
   Copyright 2009 Brain Research Institute, Melbourne, Australia

   Written by J-Donald Tournier, 13/11/09.

   This file is part of MRtrix.

   MRtrix is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   MRtrix is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "opengl/gl.h"

#include <QMessageBox>
#include <QAction>
#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QGLWidget>

#ifdef None
# undef None
#endif

#include "app.h"
#include "icon.h"
#include "dialog/file.h"
#include "dialog/opengl.h"
#include "mrview/window.h"
#include "mrview/mode/base.h"
#include "mrview/tool/base.h"


namespace MR {
  namespace Viewer {

    class Window::GLArea : public QGLWidget {
      public:
        GLArea (Window& parent) : 
          QGLWidget (QGLFormat (QGL::DoubleBuffer | QGL::DepthBuffer | QGL::Rgba), &parent),
          main (parent) { }
        QSize minimumSizeHint () const { return QSize (256, 256); }
        QSize sizeHint () const { return QSize (256, 256); }

      private:
        Window& main;

        void initializeGL () { main.initGL(); }
        void paintGL () { main.paintGL(); }
        void resizeGL (int width, int height) { main.resizeGL (width, height); }

        void mousePressEvent (QMouseEvent* event) { main.mousePressEventGL (event); }
        void mouseMoveEvent (QMouseEvent* event) { main.mouseMoveEventGL (event); }
        void mouseDoubleClickEvent (QMouseEvent* event) { main.mouseDoubleClickEventGL (event); }
        void mouseReleaseEvent (QMouseEvent* event) { main.mouseReleaseEventGL (event); }
        void wheelEvent (QWheelEvent* event) { main.wheelEventGL (event); }
    };



    Window::Window() : 
      glarea (new GLArea (*this)),
      mode (NULL) 
    { 
      setWindowTitle (tr("MRView"));
      setWindowIcon (get_icon());
      setMinimumSize (256, 256);
      setCentralWidget (glarea);


      // File actions:
      open_action = new QAction (tr("&Open"), this);
      open_action->setShortcut (tr("Ctrl+O"));
      open_action->setStatusTip (tr("Open an existing image"));
      connect (open_action, SIGNAL (triggered()), this, SLOT (open()));

      save_action = new QAction(tr("&Save"), this);
      save_action->setShortcut (tr("Ctrl+S"));
      save_action->setStatusTip (tr("Save the current image"));
      connect (save_action, SIGNAL (triggered()), this, SLOT (save()));

      properties_action = new QAction(tr("&Properties"), this);
      properties_action->setStatusTip (tr("Display the properties of the current image"));
      connect (properties_action, SIGNAL (triggered()), this, SLOT (properties()));

      quit_action = new QAction(tr("&Quit"), this);
      quit_action->setShortcut (tr("Ctrl+Q"));
      quit_action->setStatusTip (tr("Exit MRView"));
      connect (quit_action, SIGNAL (triggered()), this, SLOT (close()));

      // File menus:
      file_menu = menuBar()->addMenu (tr("&File"));
      file_menu->addAction (open_action);
      file_menu->addAction (save_action);
      file_menu->addSeparator();
      file_menu->addAction (properties_action);
      file_menu->addSeparator();
      file_menu->addAction (quit_action);

      // View actions:
      reset_windowing_action = new QAction(tr("Reset &Windowing"), this);
      reset_windowing_action->setShortcut (tr("Home"));
      reset_windowing_action->setStatusTip (tr("Reset image brightness & contrast"));
      connect (reset_windowing_action, SIGNAL (triggered()), this, SLOT (reset_windowing()));

      full_screen_action = new QAction(tr("F&ull Screen"), this);
      full_screen_action->setCheckable (true);
      full_screen_action->setChecked (false);
      full_screen_action->setShortcut (tr("F11"));
      full_screen_action->setStatusTip (tr("Toggle full screen mode"));
      connect (full_screen_action, SIGNAL (triggered()), this, SLOT (full_screen()));

      // View menu:
      view_menu = menuBar()->addMenu (tr("&View"));
      size_t num_modes;
      for (num_modes = 0; Mode::name (num_modes); ++num_modes);
      assert (num_modes > 1);
      mode_actions = new QAction* [num_modes];
      mode_group = new QActionGroup (this);
      mode_group->setExclusive (true);

      for (size_t n = 0; n < num_modes; ++n) {
        mode_actions[n] = new QAction (tr(Mode::name (n)), this);
        mode_actions[n]->setCheckable (num_modes > 1);
        mode_actions[n]->setShortcut (tr(std::string ("F"+str(n+1)).c_str()));
        mode_actions[n]->setStatusTip (tr(Mode::tooltip (n)));
        mode_group->addAction (mode_actions[n]);
        view_menu->addAction (mode_actions[n]);
      }
      mode_actions[0]->setChecked (true);
      connect (mode_group, SIGNAL (triggered(QAction*)), this, SLOT (select_mode(QAction*)));
      view_menu->addSeparator();

      view_menu_mode_area = view_menu->addSeparator();
      view_menu->addAction (reset_windowing_action);
      view_menu->addSeparator();

      view_menu->addSeparator();
      view_menu->addAction (full_screen_action);


      // Tool menu:
      tool_menu = menuBar()->addMenu (tr("&Tools"));
      size_t num_tools = Tool::count();
      for (size_t n = 0; n < num_tools; ++n) {
        Tool::Base* tool = Tool::create (*this, n);
        addDockWidget (Qt::RightDockWidgetArea, tool);
        tool_menu->addAction (tool->toggleViewAction());
      }

      // Image menu:

      next_image_action = new QAction(tr("&Next image"), this);
      next_image_action->setShortcut (tr("PgUp"));
      next_image_action->setStatusTip (tr("View the next image in the list"));
      connect (next_image_action, SIGNAL (triggered()), this, SLOT (next_image()));

      prev_image_action = new QAction(tr("&Previous image"), this);
      prev_image_action->setShortcut (tr("PgDown"));
      prev_image_action->setStatusTip (tr("View the previous image in the list"));
      connect (prev_image_action, SIGNAL (triggered()), this, SLOT (previous_image()));

      image_group = new QActionGroup (this);
      image_group->setExclusive (true);
      connect (image_group, SIGNAL (triggered(QAction*)), this, SLOT (select_image(QAction*)));

      image_menu = menuBar()->addMenu (tr("&Image"));
      image_menu->addAction (next_image_action);
      image_menu->addAction (prev_image_action);
      image_menu->addSeparator();

      menuBar()->addSeparator();


      // Help actions:
      OpenGL_action = new QAction(tr("&OpenGL Info"), this);
      OpenGL_action->setStatusTip (tr("Display OpenGL information"));
      connect (OpenGL_action, SIGNAL (triggered()), this, SLOT (OpenGL()));

      about_action = new QAction(tr("&About"), this);
      about_action->setStatusTip (tr("Display information about MRView"));
      connect (about_action, SIGNAL (triggered()), this, SLOT (about()));

      aboutQt_action = new QAction(tr("about &Qt"), this);
      aboutQt_action->setStatusTip (tr("Display information about Qt"));
      connect (aboutQt_action, SIGNAL (triggered()), this, SLOT (aboutQt()));

      // Help menu:
      help_menu = menuBar()->addMenu (tr("&Help"));
      help_menu->addAction (OpenGL_action);
      help_menu->addAction (about_action);
      help_menu->addAction (aboutQt_action);


      // StatusBar:
      statusBar()->showMessage(tr("Ready"));

      select_mode (mode_actions[0]);
    }




    Window::~Window () 
    {
      delete glarea;
    }







    void Window::open () 
    { 
      Dialog::File dialog (this, "Select images to open", true, true); 
      if (dialog.exec()) {
        VecPtr<MR::Image::Header> list;
        dialog.get_images (list);
        add_images (list);
      }
    }



    void Window::add_images (VecPtr<MR::Image::Header>& list)
    {
      for (size_t i = 0; i < list.size(); ++i) {
        QAction* action = new Image (*this, list.release (i));
        image_group->addAction (action);
        if (!i) action->setChecked (true);
      }
    }



    void Window::save () { TEST; }
    void Window::properties () { TEST; }

    void Window::select_mode (QAction* action) 
    {
      delete mode;
      size_t n = 0;
      while (action != mode_actions[n]) {
        assert (Mode::name (n) != NULL);
        ++n;
      }
      mode = Mode::create (*this, n);
    }

    void Window::reset_windowing () { Image* image = current_image(); if (image) image->reset_windowing(); }
    void Window::full_screen () { if (full_screen_action->isChecked()) showFullScreen(); else showNormal(); }

    void Window::next_image () 
    { 
      QAction* action = image_group->checkedAction();
      QList<QAction*> list = image_group->actions();
      for (int n = 0; n < list.size(); ++n) {
        if (action == list[n]) {
          list[(n+1)%list.size()]->setChecked (true);
          return;
        }
      }
    }

    void Window::previous_image ()
    { 
      QAction* action = image_group->checkedAction();
      QList<QAction*> list = image_group->actions();
      for (int n = 0; n < list.size(); ++n) {
        if (action == list[n]) {
          list[(n+list.size()-1)%list.size()]->setChecked (true);
          return;
        }
      }
    }


    void Window::select_image (QAction* action) { action->setChecked (true); }

    void Window::OpenGL () { Dialog::OpenGL gl (this); gl.exec(); }

    void Window::about () { 
      std::string message = printf ("<h1>MRView</h1>The MRtrix viewer, version %zu.%zu.%zu<br><em>%d bit %s version, built " __DATE__ "</em><p>Author: %s<p><em>%s</em>",
          App::version[0], App::version[1], App::version[2], int(8*sizeof(size_t)), 
#ifdef NDEBUG
          "release"
#else
          "debug"
#endif
          , App::author, App::copyright);

      QMessageBox::about(this, tr("About MRView"), message.c_str());
    }
    void Window::aboutQt () { QMessageBox::aboutQt (this); }




    inline void Window::paintGL () 
    {
      glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glLoadIdentity();
      mode->paint(); 
      DEBUG_OPENGL;
    }

    inline void Window::initGL () 
    { 
      GL::init ();

      CHECK_GL_EXTENSION (ARB_fragment_shader);
      CHECK_GL_EXTENSION (ARB_vertex_shader);
      CHECK_GL_EXTENSION (ARB_geometry_shader4);
      CHECK_GL_EXTENSION (EXT_texture3D);
      CHECK_GL_EXTENSION (ARB_texture_non_power_of_two);
      CHECK_GL_EXTENSION (ARB_framebuffer_object);

      GLint max_num;
      glGetIntegerv (GL_MAX_GEOMETRY_OUTPUT_VERTICES_ARB, &max_num);
      info ("maximum number of vertices for geometry shader: " + str(max_num));

      glClearColor (0.0, 0.0, 0.0, 0.0);
      glEnable (GL_DEPTH_TEST);

      DEBUG_OPENGL;
    }



    inline void Window::resizeGL (int width, int height) 
    {
      glViewport (0, 0, width, height); 
    }

    inline void Window::mousePressEventGL (QMouseEvent* event) { mode->mousePressEvent (event); }
    inline void Window::mouseMoveEventGL (QMouseEvent* event) { mode->mouseMoveEvent (event); }
    inline void Window::mouseDoubleClickEventGL (QMouseEvent* event) { mode->mouseDoubleClickEvent (event); }
    inline void Window::mouseReleaseEventGL (QMouseEvent* event) { mode->mouseReleaseEvent (event); }
    inline void Window::wheelEventGL (QWheelEvent* event) { mode->wheelEvent (event); }




  }
}



